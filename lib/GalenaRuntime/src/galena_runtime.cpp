#include "galena_runtime.hpp"

#include <exception>
#include <memory>
#include <string>

#include "generated_modules.hpp"
#include "System/errors.hpp"
#include "System/interpreter.hpp"
#include "System/module.hpp"
#include "System/parser.hpp"
#include "System/platform.hpp"
#include "System/syntax_error.hpp"
#include "System/tokenizer.hpp"

namespace galena {


namespace {

// Timing behind RSSystem.cpu_load() and RSSystem.uptime().
//
// Both describe the running program rather than the machine, so the runtime
// keeps them and the module only reads what it is handed. They are counted here
// rather than inside the interpreter because the module has no way to read a
// clock: platform.hpp hands the measurements over through set_program_clock(),
// and a module cannot take them as constructor arguments because
// scripts/generate_modules.py can only default-construct what it finds.
//
// cpu_seconds() covers tokenizing, parsing, the top level and every PROCESS
// frame, which is all the time this process spends working. What it does not
// cover is time spent asleep between frames. With the entry points as they are
// today nothing sleeps, so the share sits near 100%, and that is the truth: the
// program never yields. It becomes a real duty cycle the moment an entry point
// paces frames the way Interpreter::run_process_forever does.
double& cpu_seconds() {
    static double seconds = 0.0;
    return seconds;
}

double& program_start() {
    static double start = 0.0;
    return start;
}

// The share of wall-clock time the interpreter spent executing, as a percentage.
// Resiris has no such number on its own: the interpreter is not the only thing on
// the chip, and the only clock it can read is a plain monotonic one.
float interpreter_cpu_load() {
    const double wall = resiris::monotonic_seconds() - program_start();
    if (wall <= 0.0) {
        return 0.0f;
    }
    return static_cast<float>((cpu_seconds() / wall) * 100.0);
}

// The name of the exception type. Every Resiris error derives from
// std::runtime_error and none of them carry a virtual name, so they are matched
// with a dynamic_cast chain instead. This is the same list the Python reference
// and the C++ port print, so the error format matches in all three places.
const char* exception_name(const std::exception& error) {
    if (dynamic_cast<const resiris::ResirisSyntaxError*>(&error) != nullptr) {
        return "ResirisSyntaxError";
    }
    if (dynamic_cast<const resiris::IntDivisionError*>(&error) != nullptr) {
        return "IntDivisionError";
    }
    if (dynamic_cast<const resiris::UnknownVariableError*>(&error) != nullptr) {
        return "UnknownVariableError";
    }
    if (dynamic_cast<const resiris::ResirisTypeError*>(&error) != nullptr) {
        return "ResirisTypeError";
    }
    if (dynamic_cast<const resiris::ConstantAssignmentError*>(&error) != nullptr) {
        return "ConstantAssignmentError";
    }
    if (dynamic_cast<const resiris::MissingValueError*>(&error) != nullptr) {
        return "MissingValueError";
    }
    if (dynamic_cast<const resiris::FunctionError*>(&error) != nullptr) {
        return "FunctionError";
    }
    if (dynamic_cast<const resiris::ModuleError*>(&error) != nullptr) {
        return "ModuleError";
    }
    return "ResirisError";
}

} // namespace


int run_program(const char* source, const RunOptions& options) {
    if (options.label != nullptr && options.label[0] != '\0') {
        resiris::write_text(std::string("Galena: ") + options.label + "\n");
    }

    try {
        // Resiris modules. The list comes from lib/Resiris/modules/*.hpp via
        // scripts/generate_modules.py, so adding a module is two new files and no
        // edit here. A module that needs the registry gets it in
        // Module::on_registered(), which the ModuleRegistry calls on construction.
        auto registry = std::make_shared<resiris::ModuleRegistry>(
            galena_generated::make_modules(), options.host_build);

        // Both counters belong to this run, so a second run in the same process
        // starts from zero rather than inheriting the first one's total.
        program_start() = resiris::monotonic_seconds();
        cpu_seconds() = 0.0;

        // RSSystem.uptime() and RSSystem.cpu_load() read these. Installed here
        // rather than in the constructor of a module, because the generated list
        // can only default-construct what it finds.
        resiris::set_program_clock({
            [] { return resiris::monotonic_seconds() - program_start(); },
            &interpreter_cpu_load,
        });

        // Tokenizing
        resiris::Tokenizer tokenizer;
        auto tokens = tokenizer.tokenize(source);

        // Parser -> AST
        resiris::Parser parser(std::move(tokens));
        resiris::Program program = parser.parse();

        // Interpreter
        resiris::Interpreter interpreter(registry);
        interpreter.run(program);
        cpu_seconds() += resiris::monotonic_seconds() - program_start();

        // Also runs the PROCESS() lifecycle and its timers.
        if (options.forever) {
            interpreter.run_process_forever();
        } else {
            for (int frame = 0; frame < options.process_frames; ++frame) {
                const double frame_start = resiris::monotonic_seconds();
                interpreter.run_process_frames(1);
                cpu_seconds() += resiris::monotonic_seconds() - frame_start;
            }
        }

        resiris::write_text("Resiris: OK\n");
        return 0;
    }
    catch (const std::exception& error) {
        const std::string message = error.what();
        // An ESP_ONLY module cannot run here, and the program is refused at its
        // include instead of continuing with the hardware calls missing. Name
        // the script, because the bare error only carries the module.
        if (message.find(resiris::kEspOnlyErrorCode) != std::string::npos &&
            options.program_name != nullptr) {
            resiris::write_text(std::string("ModuleError: ") +
                                options.program_name +
                                " contains an ESP_ONLY module! On PC it can't run!\n");
            return 1;
        }
        resiris::write_text(std::string(exception_name(error)) + ": " +
                            message + "\n");
        return 1;
    }
}

} // namespace galena