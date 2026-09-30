#include "resiris/platform.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

namespace resiris {

namespace {

TextSink text_sink = nullptr;

} // namespace

void set_text_sink(TextSink sink) {
    text_sink = sink;
}

void write_text(const std::string& text) {
    if (text_sink != nullptr) {
        text_sink(text);
        return;
    }
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
}

double monotonic_seconds() {
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void sleep_seconds(double seconds) {
    const auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(seconds));
    if (duration > std::chrono::nanoseconds(0)) {
        std::this_thread::sleep_for(duration);
    }
}

} // namespace resiris
