#pragma once

namespace galena {

struct RunOptions {
    // Printed as a "Galena: <label>" banner. Null or empty means no banner, which is
    // what you want on a host while running an arbitrary file.
    const char* label = nullptr;

    // How many PROCESS() lifecycle frames to run after the top level. The ESP32
    // entry point runs six, and the host runs the same six, so a program behaves
    // identically in both places. Zero skips the lifecycle entirely.
    int process_frames = 6;
};

// Runs a Resiris program to completion: tokenizing, parsing, the top level, then
// the PROCESS() lifecycle.
//
// Every byte the program emits goes out through resiris::write_text, so the
// caller chooses where the output lands without changing how the program runs:
// the ESP32 entry point installs a Serial sink, and the host build keeps the
// default stdout sink. Both targets call this one function, and that is what
// makes their behaviour identical rather than merely similar.
//
// Returns 0 if the program ran to completion, 1 if it threw.
int run_program(const char* source, const RunOptions& options);

} // namespace galena