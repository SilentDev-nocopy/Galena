#include <Arduino.h>

#include <memory>
#include <string>
#include <vector>

#include "generated_programs.hpp"
#include "resiris/interpreter.hpp"
#include "resiris/parser.hpp"
#include "resiris/platform.hpp"
#include "resiris/rsbase.hpp"
#include "resiris/rsmath.hpp"
#include "resiris/tokenizer.hpp"

using namespace resiris;
using namespace resiris_programs;

namespace {

void serial_sink(const std::string& text) {
    Serial.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    Serial.flush();
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);

    set_text_sink(serial_sink);

    // A futtatandó Resiris program kiválasztása (build-time, a programs/*.resy
    // alapján; másik .resy programhoz itt az azonosítóját kell megadni).
    const char* source = test_resy;

    Serial.print("Galena: ");
    Serial.println(resy_program_name(source));

    try {
        // Resiris modulok
        auto modules = std::vector<std::shared_ptr<Module>>{
            std::make_shared<RsBaseModule>(),
            std::make_shared<RsMathModule>()
        };

        auto registry =
            std::make_shared<ModuleRegistry>(std::move(modules));

        // Tokenizálás
        Tokenizer tokenizer;
        auto tokens = tokenizer.tokenize(source);

        // Parser -> AST
        Parser parser(std::move(tokens));
        Program program = parser.parse();

        // Interpreter
        Interpreter interpreter(registry);
        interpreter.run(program);

        // A PROCESS() lifecycle-et és a timereket is futtatja; a test.resy
        // programnak nincs PROCESS blokkja, ezért ez nála nem csinál semmit.
        interpreter.run_process_frames(6);

        Serial.println("Resiris: OK");
    }
    catch (const std::exception& error) {
        Serial.print("Resiris error: ");
        Serial.println(error.what());
    }

    set_text_sink(nullptr);
}

void loop() {
}