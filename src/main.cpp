#include <Arduino.h>

#include <memory>
#include <vector>

#include "resiris/interpreter.hpp"
#include "resiris/parser.hpp"
#include "resiris/rsbase.hpp"
#include "resiris/rsmath.hpp"
#include "resiris/tokenizer.hpp"

using namespace resiris;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Galena boot");
    Serial.println("Testing Resiris...");

    try {
        auto modules = std::vector<std::shared_ptr<Module>>{
            std::make_shared<RsBaseModule>(),
            std::make_shared<RsMathModule>()
        };

        auto registry = std::make_shared<ModuleRegistry>(std::move(modules));

        Tokenizer tokenizer;

        const std::string source =
            "v x int = 42\n";

        auto tokens = tokenizer.tokenize(source);
        Parser parser(std::move(tokens));
        Program program = parser.parse();

        Interpreter interpreter(registry);
        interpreter.run(program);

        Serial.println("Resiris test: OK");
    }
    catch (const std::exception& error) {
        Serial.print("Resiris error: ");
        Serial.println(error.what());
    }
}

void loop() {
}