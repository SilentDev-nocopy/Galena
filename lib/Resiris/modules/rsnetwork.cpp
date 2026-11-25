#include "modules/rsnetwork.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#if defined(ARDUINO)
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace resiris {

namespace {

void require_count(const std::vector<Value>& args, std::size_t count) {
    if (args.size() != count) {
        throw ModuleSignatureError{};
    }
}

[[noreturn]] void wifi_unavailable(const std::string& fn) {
    throw ModuleError("RSNetwork." + fn +
                      ": Wi-Fi is ESP32 only and unavailable on a PC");
}

bool is_wifi_method(const std::string& fn) {
    return fn == "get_all_networks" || fn == "connect" || fn == "set_route_mode" ||
           fn == "get_ip" || fn == "get_ssid" || fn == "disconnect" ||
           fn == "get_route_mode" || fn == "set_route_info" ||
           fn == "get_own_route_info" || fn == "get_global_speed";
}

std::shared_ptr<HtmlServerState> require_server(
    const std::shared_ptr<FrameAwareState>& handle) {
    if (!handle) {
        throw ModuleError("module object handle is missing");
    }
    auto state = std::dynamic_pointer_cast<HtmlServerState>(handle);
    if (!state) {
        throw ModuleError("RSNetwork method requires an HTML server handle");
    }
    return state;
}

std::string url_decode(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < text.size() &&
                   std::isxdigit(static_cast<unsigned char>(text[i + 1])) &&
                   std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
            const char digits[] = {text[i + 1], text[i + 2], '\0'};
            out += static_cast<char>(std::strtol(digits, nullptr, 16));
            i += 2;
        } else {
            out += c;
        }
    }
    return out;
}

// The client script sends /call/<name>?value=<text>. Only `value` carries data.
std::string query_value(const std::string& target) {
    const auto question = target.find('?');
    if (question == std::string::npos) {
        return "";
    }
    const std::string query = target.substr(question + 1);
    std::size_t start = 0;
    while (start <= query.size()) {
        std::size_t end = query.find('&', start);
        if (end == std::string::npos) {
            end = query.size();
        }
        const std::string pair = query.substr(start, end - start);
        const auto equals = pair.find('=');
        if (equals != std::string::npos && pair.substr(0, equals) == "value") {
            return url_decode(pair.substr(equals + 1));
        }
        if (end == query.size()) {
            break;
        }
        start = end + 1;
    }
    return "";
}

std::string http_response(const std::string& status, const std::string& content_type,
                          const std::string& body) {
    return "HTTP/1.1 " + status + "\r\n" +
           "Content-Type: " + content_type + "\r\n" +
           "Content-Length: " + std::to_string(body.size()) + "\r\n" +
           "Connection: close\r\n\r\n" + body;
}

bool parse_int(const std::string& text, long long& out) {
    char* end = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    out = value;
    return true;
}

bool parse_float(const std::string& text, double& out) {
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    out = value;
    return true;
}

std::string lower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::string read_request(const HtmlServerState& state, const std::string& fn,
                         const std::string& type) {
    if (!state.has_request) {
        throw ModuleError("RSNetwork." + fn + ": no most recent " + type + " data");
    }
    return state.request_value;
}

void require_awaiting_response(const HtmlServerState& state, const std::string& fn) {
    if (!state.has_request) {
        throw ModuleError("RSNetwork." + fn + ": no request awaiting a response");
    }
}

#if !defined(ARDUINO)

// Host transport: one non-blocking loopback socket at a time, as in the CLI.
class PosixTransport final : public ServerTransport {
public:
    explicit PosixTransport(std::shared_ptr<NetworkTraffic> traffic)
        : traffic_(std::move(traffic)) {}

    ~PosixTransport() override {
        close_client();
        if (listen_fd_ >= 0) {
            ::close(listen_fd_);
        }
    }

    bool start(int port, std::string& error) override {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            error = std::strerror(errno);
            return false;
        }
        int reuse = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<std::uint16_t>(port));
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            error = std::strerror(errno);
            ::close(fd);
            return false;
        }
        if (::listen(fd, 8) != 0) {
            error = std::strerror(errno);
            ::close(fd);
            return false;
        }
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
        listen_fd_ = fd;
        return true;
    }

    bool read_request(std::string& request) override {
        sockaddr_in peer{};
        socklen_t peer_length = sizeof(peer);
        const int fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&peer), &peer_length);
        if (fd < 0) {
            return false;
        }
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
        pollfd descriptor{};
        descriptor.fd = fd;
        descriptor.events = POLLIN;
        if (::poll(&descriptor, 1, 20) <= 0 || (descriptor.revents & POLLIN) == 0) {
            ::close(fd);
            return false;
        }
        char buffer[8192];
        const ssize_t received = ::recv(fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        if (received <= 0) {
            ::close(fd);
            return false;
        }
        request.assign(buffer, static_cast<std::size_t>(received));
        traffic_->bytes.fetch_add(static_cast<std::uint64_t>(received));
        client_fd_ = fd;
        return true;
    }

    void send(const std::string& bytes) override {
        if (client_fd_ < 0) {
            return;
        }
        std::size_t sent = 0;
        while (sent < bytes.size()) {
            const ssize_t result =
                ::send(client_fd_, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
            if (result <= 0) {
                break;
            }
            sent += static_cast<std::size_t>(result);
        }
        traffic_->bytes.fetch_add(sent);
    }

    void close_client() override {
        if (client_fd_ >= 0) {
            ::close(client_fd_);
            client_fd_ = -1;
        }
    }

private:
    std::shared_ptr<NetworkTraffic> traffic_;
    int listen_fd_ = -1;
    int client_fd_ = -1;
};

#else

// Device transport: WiFiServer hands out a WiFiClient for the one request the
// server keeps. Reads never wait for more bytes than have already arrived.
class ArduinoTransport final : public ServerTransport {
public:
    explicit ArduinoTransport(std::shared_ptr<NetworkTraffic> traffic)
        : traffic_(std::move(traffic)) {}

    ~ArduinoTransport() override {
        close_client();
        if (server_ != nullptr) {
            server_->close();
        }
    }

    bool start(int port, std::string& error) override {
        // lwIP's tcpip task does not exist until a Wi-Fi mode has been started.
        // Calling WiFiServer::begin() before that asserts in tcpip.c and resets
        // the chip. Station mode brings the stack up without joining a network;
        // a program that wants AP or MB should call set_route_mode() first.
        wifi_mode_t mode = WIFI_MODE_NULL;
        if (esp_wifi_get_mode(&mode) != ESP_OK || mode == WIFI_MODE_NULL) {
            WiFi.persistent(false);
            if (!WiFi.mode(WIFI_STA)) {
                error = "could not start the Wi-Fi stack";
                return false;
            }
        }

        server_.reset(new WiFiServer(port));
        server_->begin();
        if (!*server_) {
            error = "WiFiServer could not bind the port";
            return false;
        }
        return true;
    }

    bool read_request(std::string& request) override {
        if (server_ == nullptr) {
            return false;
        }
        WiFiClient client = server_->available();
        if (!client.connected()) {
            return false;
        }
        std::string data;
        while (client.available() > 0 && data.size() < 8192) {
            std::uint8_t buffer[256];
            const std::size_t chunk =
                std::min<std::size_t>(sizeof(buffer), 8192 - data.size());
            const int received = static_cast<int>(client.read(buffer, chunk));
            if (received <= 0) {
                break;
            }
            data.append(reinterpret_cast<const char*>(buffer),
                        static_cast<std::size_t>(received));
        }
        if (data.empty()) {
            client.stop();
            return false;
        }
        traffic_->bytes.fetch_add(static_cast<std::uint64_t>(data.size()));
        client_ = client;
        request = std::move(data);
        return true;
    }

    void send(const std::string& bytes) override {
        if (!client_.connected()) {
            return;
        }
        std::size_t sent = 0;
        while (sent < bytes.size()) {
            const std::size_t chunk = std::min<std::size_t>(bytes.size() - sent, 1024);
            const std::size_t written = client_.write(
                reinterpret_cast<const std::uint8_t*>(bytes.data() + sent), chunk);
            if (written == 0) {
                break;
            }
            sent += written;
        }
        traffic_->bytes.fetch_add(sent);
    }

    void close_client() override {
        if (client_.connected()) {
            client_.stop();
        }
        client_ = WiFiClient();
    }

private:
    std::shared_ptr<NetworkTraffic> traffic_;
    std::unique_ptr<WiFiServer> server_;
    WiFiClient client_;
};

#endif

std::unique_ptr<ServerTransport> make_transport(std::shared_ptr<NetworkTraffic> traffic) {
#if defined(ARDUINO)
    return std::unique_ptr<ServerTransport>(new ArduinoTransport(std::move(traffic)));
#else
    return std::unique_ptr<ServerTransport>(new PosixTransport(std::move(traffic)));
#endif
}

#if defined(ARDUINO)

// wifi_config_t holds fixed-size uint8_t arrays that are not guaranteed to end
// in a NUL when the field is full.
std::string bounded_string(const std::uint8_t* data, std::size_t capacity) {
    std::size_t length = 0;
    while (length < capacity && data[length] != 0) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(data), length);
}

std::string own_ssid(const WifiState& state) {
    if (!state.own_ssid.empty()) {
        return state.own_ssid;
    }
    wifi_config_t config{};
    if (esp_wifi_get_config(WIFI_IF_AP, &config) == ESP_OK) {
        return bounded_string(config.ap.ssid, sizeof(config.ap.ssid));
    }
    return "";
}

std::string mask_password(const std::string& password) {
    if (password.empty()) {
        return "***";
    }
    if (password.size() == 1) {
        return password + "***" + password;
    }
    return password.substr(0, 1) + "***" + password.substr(password.size() - 1);
}

Value call_wifi_method(const std::string& fn,
                       const std::shared_ptr<FrameAwareState>& handle,
                       const std::vector<Value>& args, WifiState& state) {
    (void)handle; // The speed handler measures time through FrameAwareState::now.

    if (fn == "get_all_networks") {
        require_count(args, 0);
        const int found = WiFi.scanNetworks();
        if (found < 0) {
            throw ModuleError("RSNetwork.get_all_networks: Wi-Fi scan failed");
        }
        std::string names;
        for (int i = 0; i < found; ++i) {
            if (i != 0) {
                names += ", ";
            }
            names += WiFi.SSID(i).c_str();
        }
        return Value::make_string(names);
    }

    if (fn == "connect") {
        require_count(args, 2);
        if (args[0].type != Value::Type::String || args[1].type != Value::Type::String) {
            throw ModuleSignatureError{};
        }
        if (state.route_mode != "CRE" && state.route_mode != "MB") {
            throw ModuleError("RSNetwork.connect: only usable in CRE or MB mode");
        }
        const std::string ssid = args[0].string_value;
        const std::string password = args[1].string_value;
        WiFi.begin(ssid.c_str(), password.c_str());
        const std::uint32_t deadline = millis() + 10000U;
        while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
            delay(100);
        }
        const bool connected = WiFi.status() == WL_CONNECTED;
        if (connected) {
            state.connected_ssid = ssid;
        }
        return Value::make_bool(connected);
    }

    if (fn == "set_route_mode") {
        require_count(args, 1);
        if (args[0].type != Value::Type::String) {
            throw ModuleSignatureError{};
        }
        const std::string& mode = args[0].string_value;
        wifi_mode_t raw;
        if (mode == "CRE") {
            raw = WIFI_STA;
        } else if (mode == "SR") {
            raw = WIFI_AP;
        } else if (mode == "MB") {
            raw = WIFI_AP_STA;
        } else {
            throw ModuleError("RSNetwork.set_route_mode: unknown route mode " + mode);
        }
        if (!WiFi.mode(raw)) {
            return Value::make_bool(false);
        }
        state.route_mode = mode;
        if ((raw == WIFI_AP || raw == WIFI_AP_STA) && !state.own_ssid.empty()) {
            WiFi.softAP(state.own_ssid.c_str(), state.own_password.c_str());
        }
        return Value::make_bool(true);
    }

    if (fn == "get_route_mode") {
        require_count(args, 0);
        return Value::make_string(state.route_mode);
    }

    if (fn == "set_route_info") {
        require_count(args, 2);
        if (args[0].type != Value::Type::String || args[1].type != Value::Type::String) {
            throw ModuleSignatureError{};
        }
        if (state.route_mode != "SR") {
            throw ModuleError("RSNetwork.set_route_info: only usable in SR mode");
        }
        const std::string ssid = args[0].string_value;
        const std::string password = args[1].string_value;
        if (password.size() < 8) {
            throw ModuleError("RSNetwork.set_route_info: access point passwords need at least 8 characters");
        }
        if (!WiFi.softAP(ssid.c_str(), password.c_str())) {
            return Value::make_bool(false);
        }
        state.own_ssid = ssid;
        state.own_password = password;
        return Value::make_bool(true);
    }

    if (fn == "get_own_route_info") {
        require_count(args, 0);
        const std::string ssid = own_ssid(state);
        if (ssid.empty()) {
            throw ModuleError("RSNetwork.get_own_route_info: no route info set");
        }
        return Value::make_string(ssid + " " + mask_password(state.own_password));
    }

    if (fn == "get_ip") {
        require_count(args, 1);
        if (args[0].type != Value::Type::String) {
            throw ModuleSignatureError{};
        }
        const std::string& ssid = args[0].string_value;
        if (!ssid.empty() && (state.route_mode == "SR" || state.route_mode == "MB") &&
            ssid == own_ssid(state)) {
            return Value::make_string(WiFi.softAPIP().toString().c_str());
        }
        if (!ssid.empty() && ssid == state.connected_ssid &&
            WiFi.status() == WL_CONNECTED) {
            return Value::make_string(WiFi.localIP().toString().c_str());
        }
        throw ModuleError("RSNetwork.get_ip: cannot resolve SSID " + ssid);
    }

    if (fn == "get_ssid") {
        require_count(args, 1);
        if (args[0].type != Value::Type::String) {
            throw ModuleSignatureError{};
        }
        const std::string& ip = args[0].string_value;
        if (WiFi.status() == WL_CONNECTED && ip == WiFi.localIP().toString().c_str()) {
            return Value::make_string(state.connected_ssid);
        }
        const std::string ssid = own_ssid(state);
        if (!ssid.empty() && (state.route_mode == "SR" || state.route_mode == "MB") &&
            ip == WiFi.softAPIP().toString().c_str()) {
            return Value::make_string(ssid);
        }
        throw ModuleError("RSNetwork.get_ssid: no network matches " + ip);
    }

    if (fn == "disconnect") {
        require_count(args, 0);
        const bool was_connected = WiFi.status() == WL_CONNECTED;
        if (was_connected) {
            WiFi.disconnect();
            state.connected_ssid.clear();
        }
        return Value::make_bool(was_connected);
    }

    if (fn == "get_global_speed") {
        require_count(args, 0);
        const auto speed = std::dynamic_pointer_cast<WifiSpeedState>(handle);
        if (!speed || !speed->traffic) {
            throw ModuleError("RSNetwork.get_global_speed: requires a Wi-Fi data handler");
        }
        const std::uint64_t bytes = speed->traffic->bytes.load();
        if (!speed->sampled) {
            speed->sampled = true;
            speed->last_now = handle->now;
            speed->last_bytes = bytes;
            return Value::make_float(0.0);
        }
        const double elapsed = handle->now - speed->last_now;
        if (elapsed <= 0.0) {
            return Value::make_float(0.0);
        }
        const double rate =
            static_cast<double>(bytes - speed->last_bytes) / elapsed;
        speed->last_now = handle->now;
        speed->last_bytes = bytes;
        return Value::make_float(rate);
    }

    throw ModuleError("RSNetwork." + fn + ": unknown Wi-Fi method");
}

#endif

} // namespace

HtmlServerState::~HtmlServerState() = default;

std::vector<std::string> RsNetworkModule::function_names() const {
    return {
        "wifi", "create_wifi_data_process_handler", "html_server_handler",
        "get_all_networks", "connect", "set_route_mode", "get_ip", "get_ssid",
        "disconnect", "get_route_mode", "set_route_info", "get_own_route_info",
        "get_global_speed",
        "create_server", "set_page_content", "set_client_script", "start_server",
        "process", "request_string_data", "request_int_data", "request_float_data",
        "request_boolean_data", "get_request_state", "respond_string_data",
        "respond_int_data", "respond_float_data", "respond_boolean_data",
    };
}

std::shared_ptr<WifiState> RsNetworkModule::wifi_state() {
    if (!wifi_) {
        wifi_ = std::make_shared<WifiState>();
#if defined(ARDUINO)
        // The module owns credentials for this run; do not write them to NVS.
        WiFi.persistent(false);
#endif
    }
    return wifi_;
}

std::shared_ptr<NetworkTraffic> RsNetworkModule::traffic_counter() {
    if (!traffic_) {
        traffic_ = std::make_shared<NetworkTraffic>();
    }
    return traffic_;
}

Value RsNetworkModule::call_function(const std::string& fn,
                                     const std::vector<Value>& args) {
    require_count(args, 0);

    if (fn == "wifi") {
#if defined(ARDUINO)
        return Value::make_module(
            std::make_shared<ModuleObject>("RSNetwork", wifi_state()));
#else
        wifi_unavailable(fn);
#endif
    }

    if (fn == "create_wifi_data_process_handler") {
#if defined(ARDUINO)
        auto speed = std::make_shared<WifiSpeedState>();
        speed->traffic = traffic_counter();
        return Value::make_module(std::make_shared<ModuleObject>("RSNetwork", speed));
#else
        wifi_unavailable(fn);
#endif
    }

    if (fn == "html_server_handler") {
        auto state = std::make_shared<HtmlServerState>();
        state->traffic = traffic_counter();
        return Value::make_module(std::make_shared<ModuleObject>("RSNetwork", state));
    }

    throw ModuleError("RSNetwork." + fn + ": unknown module function");
}

Value RsNetworkModule::call_object_method(
    const std::string& fn, const std::shared_ptr<FrameAwareState>& handle,
    const std::vector<Value>& args) {
    if (is_wifi_method(fn)) {
#if defined(ARDUINO)
        return call_wifi_method(fn, handle, args, *wifi_state());
#else
        wifi_unavailable(fn);
#endif
    }

    const auto server = require_server(handle);

    if (fn == "create_server") {
        require_count(args, 1);
        if (args[0].type != Value::Type::Int) {
            throw ModuleSignatureError{};
        }
        if (server->created) {
            throw ModuleError("RSNetwork.create_server: server already created");
        }
        const long long port = args[0].int_value;
        if (port < 1 || port > 65535) {
            throw ModuleError("RSNetwork.create_server: port must be between 1 and 65535");
        }
        server->port = static_cast<int>(port);
        server->created = true;
        return Value::make_bool(true);
    }

    if (fn == "set_page_content") {
        require_count(args, 1);
        if (args[0].type != Value::Type::String) {
            throw ModuleSignatureError{};
        }
        server->page_content = args[0].string_value;
        return Value::make_bool(true);
    }

    if (fn == "set_client_script") {
        require_count(args, 1);
        if (args[0].type != Value::Type::String) {
            throw ModuleSignatureError{};
        }
        server->client_script = args[0].string_value;
        return Value::make_bool(true);
    }

    if (fn == "start_server") {
        require_count(args, 0);
        if (!server->created) {
            throw ModuleError("RSNetwork.start_server: call create_server(port_number) first");
        }
        if (server->started) {
            throw ModuleError("RSNetwork.start_server: server already started");
        }
        server->transport = make_transport(server->traffic);
        std::string error;
        if (!server->transport->start(server->port, error)) {
            throw ModuleError("RSNetwork.start_server: could not listen on port " +
                              std::to_string(server->port) +
                              (error.empty() ? std::string() : ": " + error));
        }
        server->started = true;
        return Value::make_bool(true);
    }

    if (fn == "process") {
        require_count(args, 0);
        if (!server->started) {
            throw ModuleError("RSNetwork.process: call start_server() first");
        }

        // First send the response buffered since the previous call.
        if (server->client_open) {
            server->transport->send(http_response("200 OK", "text/plain; charset=utf-8",
                                                  server->pending_response));
            server->transport->close_client();
            server->client_open = false;
        }
        server->has_request = false;
        server->request_read = false;
        server->request_value.clear();
        server->pending_response.clear();

        // Then accept at most one new request.
        std::string request;
        if (!server->transport->read_request(request)) {
            return Value::make_bool(false);
        }
        const auto first_space = request.find(' ');
        const auto second_space = first_space == std::string::npos
                                      ? std::string::npos
                                      : request.find(' ', first_space + 1);
        const std::string target =
            second_space == std::string::npos
                ? std::string()
                : request.substr(first_space + 1, second_space - first_space - 1);

        if (target.rfind("/call/", 0) == 0) {
            server->has_request = true;
            server->request_read = false;
            server->request_value = query_value(target);
            server->client_open = true;
            return Value::make_bool(true);
        }
        if (target == "/" || target == "/index.html") {
            server->transport->send(
                http_response("200 OK", "text/html; charset=utf-8", server->page_content));
        } else if (target == "/rsclient.js") {
            server->transport->send(http_response(
                "200 OK", "application/javascript; charset=utf-8", server->client_script));
        } else {
            server->transport->send(
                http_response("404 Not Found", "text/plain; charset=utf-8", "Not Found"));
        }
        server->transport->close_client();
        return Value::make_bool(true);
    }

    if (fn == "request_string_data") {
        require_count(args, 0);
        server->request_read = true;
        return Value::make_string(read_request(*server, fn, "string"));
    }

    if (fn == "request_int_data") {
        require_count(args, 0);
        const std::string text = read_request(*server, fn, "int");
        long long number = 0;
        if (!parse_int(text, number)) {
            throw ModuleError("RSNetwork.request_int_data: no most recent int data");
        }
        server->request_read = true;
        return Value::make_int(number);
    }

    if (fn == "request_float_data") {
        require_count(args, 0);
        const std::string text = read_request(*server, fn, "float");
        double number = 0.0;
        if (!parse_float(text, number)) {
            throw ModuleError("RSNetwork.request_float_data: no most recent float data");
        }
        server->request_read = true;
        return Value::make_float(number);
    }

    if (fn == "request_boolean_data") {
        require_count(args, 0);
        const std::string text = lower(read_request(*server, fn, "boolean"));
        if (text == "true" || text == "1") {
            server->request_read = true;
            return Value::make_bool(true);
        }
        if (text == "false" || text == "0") {
            server->request_read = true;
            return Value::make_bool(false);
        }
        throw ModuleError("RSNetwork.request_boolean_data: no most recent boolean data");
    }

    if (fn == "get_request_state") {
        require_count(args, 0);
        return Value::make_bool(server->has_request && !server->request_read);
    }

    if (fn == "respond_string_data" || fn == "respond_int_data" ||
        fn == "respond_float_data" || fn == "respond_boolean_data") {
        require_count(args, 1);
        require_awaiting_response(*server, fn);

        Value::Type expected = Value::Type::String;
        if (fn == "respond_int_data") {
            expected = Value::Type::Int;
        } else if (fn == "respond_float_data") {
            expected = Value::Type::Float;
        } else if (fn == "respond_boolean_data") {
            expected = Value::Type::Bool;
        }
        if (args[0].type != expected) {
            throw ModuleSignatureError{};
        }

        switch (args[0].type) {
            case Value::Type::String:
                server->pending_response = args[0].string_value;
                break;
            case Value::Type::Int:
                server->pending_response = std::to_string(args[0].int_value);
                break;
            case Value::Type::Float:
                server->pending_response = float_to_string(args[0].float_value);
                break;
            case Value::Type::Bool:
                server->pending_response = args[0].bool_value ? "true" : "false";
                break;
            default:
                throw ModuleSignatureError{};
        }
        return Value::make_bool(true);
    }

    throw ModuleError("RSNetwork." + fn + ": unknown module function");
}

Value RsNetworkModule::get_constant(const std::string& name) const {
    if (name == "ROUTEMODE_CRE") {
        return Value::make_string("CRE");
    }
    if (name == "ROUTEMODE_SR") {
        return Value::make_string("SR");
    }
    if (name == "ROUTEMODE_MB") {
        return Value::make_string("MB");
    }
    throw ModuleError("RSNetwork." + name + ": unknown module constant");
}

} // namespace resiris
