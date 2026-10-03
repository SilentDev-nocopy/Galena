#include <Arduino.h>

#include <string>

#include "galena_runtime.hpp"
#include "generated_programs.hpp"
#include "resiris/platform.hpp"

using namespace resiris;

namespace {

void serial_sink(const std::string& text) {
    Serial.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    Serial.flush();
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);

    resiris::set_text_sink(serial_sink);

    // The Resiris program to run, chosen at build time from programs/*.resy. To
    // run a different one, name its identifier here. scripts/generate_programs.py
    // rewrites this line, and the host's --embedded mode reads it back.
    const char* source = resiris_programs::recursion_depth_resy;

    galena::run_program(source, galena::RunOptions{
                                   .label =
                                       resiris_programs::resy_program_name(source),
                                   // The device runs every module, so the ESP_ONLY
                                   // check stays off here; host_build defaults to false.
                                   .program_name =
                                       resiris_programs::resy_program_name(source),
                                   .process_frames = 6,
                               });

    resiris::set_text_sink(nullptr);
}

void loop() {
}