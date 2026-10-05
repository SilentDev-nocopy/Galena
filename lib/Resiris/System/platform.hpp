#pragma once

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

} // namespace resiris
