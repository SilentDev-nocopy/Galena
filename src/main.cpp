#include <Arduino.h>

#include <memory>
#include <string>
#include <vector>

#include "resiris/interpreter.hpp"
#include "resiris/parser.hpp"
#include "resiris/platform.hpp"
#include "resiris/rsbase.hpp"
#include "resiris/rsmath.hpp"
#include "resiris/tokenizer.hpp"

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

    Serial.println("Galena boot");
    Serial.println("Testing Resiris...");

    set_text_sink(serial_sink);

    try {
        // Resiris modulok
        auto modules = std::vector<std::shared_ptr<Module>>{
            std::make_shared<RsBaseModule>(),
            std::make_shared<RsMathModule>()
        };

        auto registry =
            std::make_shared<ModuleRegistry>(std::move(modules));

        // -------------------------------------------------
        // Resiris szintaxis es modul funkcio teszt
        // -------------------------------------------------

        const std::string source =
            "<include> RSMath, RSBase\n"
            "\n"
            "c FPS float = 10.0\n"
            "c LIMIT int = 10\n"
            "\n"
            "v a int = 10\n"
            "v b int = 3\n"
            "v x float = 2.5\n"
            "v y float = 4.0\n"
            "v s string = \"Hello\"\n"
            "v t string = \"World\"\n"
            "v flag bool = true\n"
            "v u UnknownObject = 42\n"
            "v index int = 0\n"
            "\n"
            "print_cmd(a)\n"
            "print_cmd(b)\n"
            "print_cmd(a + b)\n"
            "print_cmd(a - b)\n"
            "print_cmd(a * b)\n"
            "print_cmd(a % b)\n"
            "print_cmd(7 % 3)\n"
            "print_cmd(-7 % 3)\n"
            "print_cmd(7 % -3)\n"
            "print_cmd(x + y)\n"
            "print_cmd(x * y)\n"
            "print_cmd(y / x)\n"
            "print_cmd(0.1 + 0.2)\n"
            "print_cmd(-x)\n"
            "print_cmd(100.0)\n"
            "print_cmd(0.00001)\n"
            "print_cmd(1000000.0)\n"
            "print_cmd(x * 1000000.0)\n"
            "print_cmd(s + \" \" + t)\n"
            "print_cmd(a == 10)\n"
            "print_cmd(a != b)\n"
            "print_cmd(b < a)\n"
            "print_cmd(s < t)\n"
            "print_cmd(flag)\n"
            "print_cmd(flag == (a > b))\n"
            "print_cmd(u.type())\n"
            "print_cmd(x.type())\n"
            "print_cmd(s.type())\n"
            "print_cmd(flag.type())\n"
            "print_cmd(x.type(int))\n"
            "print_cmd(b.type(float))\n"
            "print_cmd(b.type(string))\n"
            "print_cmd(u.type(bool))\n"
            "print_cmd(b.type(bool))\n"
            "print_cmd(s.string())\n"
            "print_cmd(str(a))\n"
            "print_cmd(LIMIT)\n"
            "\n"
            "v dec float = 26.0\n"
            "dec += 5\n"
            "print_cmd(dec)\n"
            "dec -= 2\n"
            "print_cmd(dec)\n"
            "dec *= 2\n"
            "print_cmd(dec)\n"
            "dec /= 2.0\n"
            "print_cmd(dec)\n"
            "\n"
            "fn fib(n):\n"
            "\tif n <= 1:\n"
            "\t\treturn n\n"
            "\treturn fib(n - 2) + fib(n - 1)\n"
            "\n"
            "fn classify(val):\n"
            "\tif val.type() == \"int\":\n"
            "\t\treturn \"int value\"\n"
            "\telif val.type() == \"float\":\n"
            "\t\treturn \"float value\"\n"
            "\telif val.type() == \"bool\":\n"
            "\t\treturn \"bool value\"\n"
            "\telse:\n"
            "\t\treturn \"string value\"\n"
            "\n"
            "fn describe(n):\n"
            "\tmat n:\n"
            "\t\t1:\n"
            "\t\t\treturn \"one\"\n"
            "\t\t2:\n"
            "\t\t\treturn \"two\"\n"
            "\t\t3:\n"
            "\t\t\treturn \"three\"\n"
            "\t\telse:\n"
            "\t\t\treturn \"many\"\n"
            "\n"
            "print_cmd(fib(10))\n"
            "print_cmd(classify(a))\n"
            "print_cmd(classify(x))\n"
            "print_cmd(classify(flag))\n"
            "print_cmd(classify(s))\n"
            "print_cmd(describe(1))\n"
            "print_cmd(describe(2))\n"
            "print_cmd(describe(7))\n"
            "\n"
            "mat x.type():\n"
            "\tint:\n"
            "\t\tprint_cmd(\"mat says int\")\n"
            "\tfloat:\n"
            "\t\tprint_cmd(\"mat says float\")\n"
            "\telse:\n"
            "\t\tprint_cmd(\"mat says other\")\n"
            "\n"
            "v counter FunctionalObject = FunctionalObject.new():\n"
            "\tindex += 1\n"
            "\treturn index\n"
            "print_cmd(counter())\n"
            "print_cmd(counter())\n"
            "print_cmd(counter())\n"
            "\n"
            "print_cmd(RSMath.abs(-7))\n"
            "print_cmd(RSMath.abs(-7.5))\n"
            "print_cmd(RSMath.sqrt(16.0))\n"
            "print_cmd(RSMath.cbrt(27.0))\n"
            "print_cmd(RSMath.pow(2.0, 10.0))\n"
            "print_cmd(RSMath.floor(3.7))\n"
            "print_cmd(RSMath.ceil(3.2))\n"
            "print_cmd(RSMath.round(3.5))\n"
            "print_cmd(RSMath.round(2.5))\n"
            "print_cmd(RSMath.ln(RSMath[E]))\n"
            "print_cmd(RSMath.log10(1000.0))\n"
            "print_cmd(RSMath.sin(0.0))\n"
            "print_cmd(RSMath.cos(0.0))\n"
            "print_cmd(RSMath.tan(0.0))\n"
            "print_cmd(RSMath.atan(1.0))\n"
            "print_cmd(RSMath.hypot(3.0, 4.0))\n"
            "print_cmd(RSMath.pythagoras(3.0, 4.0))\n"
            "print_cmd(RSMath.factorial(10))\n"
            "print_cmd(RSMath.ncr(5, 2))\n"
            "print_cmd(RSMath.npr(5, 2))\n"
            "print_cmd(RSMath.gcd(12, 18))\n"
            "print_cmd(RSMath.lcm(4, 6))\n"
            "print_cmd(RSMath.mod(7, 3))\n"
            "print_cmd(RSMath.mod(-7, 3))\n"
            "print_cmd(RSMath.min(2, 7))\n"
            "print_cmd(RSMath.max(2, 7))\n"
            "print_cmd(RSMath.clamp(5.5, 1.0, 3.0))\n"
            "print_cmd(RSMath.is_nan(1.0))\n"
            "print_cmd(RSMath.is_inf(1.0))\n"
            "print_cmd(RSMath.is_finite(1.0))\n"
            "print_cmd(RSMath[PI])\n"
            "print_cmd(RSMath[E])\n"
            "\n"
            "v timer ModuleObject = RSBase.await()\n"
            "timer.set_timer(0.35)\n"
            "timer.on_timeout(\"timer_callback\")\n"
            "START():\n"
            "\tprint_cmd(\"start lifecycle\")\n"
            "PROCESS(FPS):\n"
            "\tprint_cmd(timer.process())\n"
            "fn timer_callback():\n"
            "\tprint_cmd(\"timer callback fired\")\n";

        Serial.println("--- Resiris program output ---");

        // Tokenizálás
        Tokenizer tokenizer;
        auto tokens = tokenizer.tokenize(source);

        // Parser
        Parser parser(std::move(tokens));
        Program program = parser.parse();

        // Interpreter
        Interpreter interpreter(registry);
        interpreter.run(program);

        Serial.println("--- Resiris PROCESS frames ---");
        interpreter.run_process_frames(6);

        Serial.println("------------------------------");
        Serial.println("Resiris test: OK");
    }
    catch (const std::exception& error) {
        Serial.print("Resiris error: ");
        Serial.println(error.what());
    }

    set_text_sink(nullptr);
}

void loop() {
}