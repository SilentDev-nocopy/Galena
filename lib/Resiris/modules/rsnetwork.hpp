#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "System/module.hpp"

namespace resiris {

// Bytes moved through this module's sockets since the program started. The
// Wi-Fi data handler samples the counter between calls and divides by the
// frame-clock time the interpreter injects into FrameAwareState::now.
struct NetworkTraffic {
    std::atomic<std::uint64_t> bytes{0};
};

// One socket at a time. The host builds a POSIX socket bound to 127.0.0.1;
// the firmware uses Arduino's WiFiServer/WiFiClient. Keeping the boundary here
// lets the HTTP rules in rsnetwork.cpp stay identical on both targets.
class ServerTransport {
public:
    virtual ~ServerTransport() = default;

    // Binds and starts listening. On failure `error` says why.
    virtual bool start(int port, std::string& error) = 0;

    // Takes at most one request from the socket, without waiting for one.
    virtual bool read_request(std::string& request) = 0;

    virtual void send(const std::string& bytes) = 0;
    virtual void close_client() = 0;
};

// State behind RSNetwork.html_server_handler(). All of it is portable; the
// target-specific socket lives behind ServerTransport.
struct HtmlServerState : FrameAwareState {
    ~HtmlServerState() override;

    int port = 0;
    bool created = false;
    bool started = false;
    bool client_open = false;

    std::string page_content;
    std::string client_script;

    // The one request the server is allowed to keep. client_open stays true
    // until the next process() call sends the buffered response.
    bool has_request = false;
    bool request_read = false;
    std::string request_value;
    std::string pending_response;

    std::shared_ptr<NetworkTraffic> traffic;
    std::unique_ptr<ServerTransport> transport;
};

// One Wi-Fi handle per program: every RSNetwork.wifi() call returns a handle
// onto the same state, so a mode chosen through one handle is visible to the
// others.
struct WifiState : FrameAwareState {
    std::string route_mode = "CRE";
    std::string own_ssid;
    std::string own_password;
    std::string connected_ssid;
};

// State behind RSNetwork.create_wifi_data_process_handler(). The speed is a
// measured byte rate, not an estimate from signal strength.
struct WifiSpeedState : FrameAwareState {
    std::shared_ptr<NetworkTraffic> traffic;
    bool sampled = false;
    double last_now = 0.0;
    std::uint64_t last_bytes = 0;
};

// Local server and ESP32 Wi-Fi control. The Wi-Fi half is hardware-only: on a
// PC the module still loads and the HTML server still works, but every Wi-Fi
// call raises a ModuleError instead of inventing a network to report on.
class RsNetworkModule : public Module {
public:
    ModuleTarget target() const override { return ModuleTarget::EspAndPc; }

    std::string module_name() const override { return "RSNetwork"; }

    std::vector<std::string> function_names() const override;

    std::map<std::string, std::string> variables() const override {
        return {
            {"ROUTEMODE_CRE", "string"},
            {"ROUTEMODE_SR", "string"},
            {"ROUTEMODE_MB", "string"},
        };
    }

    Value call_function(const std::string& fn, const std::vector<Value>& args) override;
    Value call_object_method(const std::string& fn,
                             const std::shared_ptr<FrameAwareState>& handle,
                             const std::vector<Value>& args) override;
    Value get_constant(const std::string& name) const override;

private:
    std::shared_ptr<WifiState> wifi_state();
    std::shared_ptr<NetworkTraffic> traffic_counter();

    std::shared_ptr<WifiState> wifi_;
    std::shared_ptr<NetworkTraffic> traffic_;
};

} // namespace resiris
