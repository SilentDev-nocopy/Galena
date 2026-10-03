#include "galena_runtime.hpp"

#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "resiris/errors.hpp"
#include "resiris/interpreter.hpp"
#include "resiris/module.hpp"
#include "resiris/parser.hpp"
#include "resiris/platform.hpp"
#include "resiris/rsbase.hpp"
#include "resiris/rsmath.hpp"
#include "resiris/syntax_error.hpp"
#include "resiris/tokenizer.hpp"

namespace galena {


namespace {

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
        // Resiris modules
        auto modules = std::vector<std::shared_ptr<resiris::Module>>{
            std::make_shared<resiris::RsBaseModule>(),
            std::make_shared<resiris::RsMathModule>()
        };

        auto registry = std::make_shared<resiris::ModuleRegistry>(
            std::move(modules), options.host_build);

        // Tokenizing
        resiris::Tokenizer tokenizer;
        auto tokens = tokenizer.tokenize(source);

        // Parser -> AST
        resiris::Parser parser(std::move(tokens));
        resiris::Program program = parser.parse();

        // Interpreter
        resiris::Interpreter interpreter(registry);
        interpreter.run(program);

        // Also runs the PROCESS() lifecycle and its timers.
        if (options.process_frames > 0) {
            interpreter.run_process_frames(options.process_frames);
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