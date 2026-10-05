#include "System/platform.hpp"

#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <utility>

namespace resiris {

namespace {

TextSink text_sink = nullptr;

ProgramClock program_clock;

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

void set_program_clock(ProgramClock clock) {
    program_clock = std::move(clock);
}

bool has_program_clock() {
    return static_cast<bool>(program_clock.seconds) &&
           static_cast<bool>(program_clock.cpu_load);
}

double program_seconds() {
    if (!program_clock.seconds) {
        throw std::runtime_error("resiris: no host installed a program clock");
    }
    return program_clock.seconds();
}

float program_cpu_load() {
    if (!program_clock.cpu_load) {
        throw std::runtime_error("resiris: no host installed a load measurement");
    }
    return program_clock.cpu_load();
}

} // namespace resiris
