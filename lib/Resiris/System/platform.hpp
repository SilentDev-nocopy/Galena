#pragma once

#include <functional>
#include <string>

namespace resiris {

// The host services a Resiris program needs from its environment: somewhere to
// write text, a monotonic clock and a way to sleep. Keeping them behind these
// three functions lets the same interpreter build for a desktop host and for
// firmware, where output goes to a serial port instead of stdout.

using TextSink = void (*)(const std::string& text);

// Installs where write_text sends its text. Passing nullptr restores the default
// sink, which writes to stdout.
void set_text_sink(TextSink sink);

void write_text(const std::string& text);

// Seconds on a monotonic clock, so frame scheduling is not affected by the
// system clock being changed underneath it.
double monotonic_seconds();

void sleep_seconds(double seconds);

// Two measurements about the program running right now, which only the host can
// make. They are installed rather than passed to a module's constructor, because
// every module has to stay default-constructible: the build generates the module
// list from the files in src/modules/ and can only ever call make_shared<>().
struct ProgramClock {
    // Seconds since this program started executing, not since boot.
    std::function<double()> seconds;
    // Percent of wall-clock time spent executing, 0..100.
    std::function<float()> cpu_load;
};

// Installs the program clock. The runtime calls this once, after creating the
// interpreter and before running the program, because that is the only point
// where the program's own start time and duty counters exist.
//
// A module asks has_program_clock() first, so it can report the missing
// measurement as a Resiris error naming its own function, instead of letting a
// C++ exception escape through the interpreter.
void set_program_clock(ProgramClock clock);

bool has_program_clock();

// Both throw std::runtime_error when no clock is installed.
double program_seconds();

float program_cpu_load();

} // namespace resiris
