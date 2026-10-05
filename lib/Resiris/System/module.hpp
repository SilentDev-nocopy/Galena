#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "System/errors.hpp"
#include "System/value.hpp"

namespace resiris {

// Thrown inside a module implementation to signal a Python-TypeError-equivalent
// signature mismatch. The registry converts it into the standard Resiris
// module error message.
class ModuleSignatureError {};

// Which targets can run a module. The device runs every module; a host build
// refuses an EspOnly module rather than running it with the hardware missing.
enum class ModuleTarget {
    EspOnly,
    EspAndPc,
};

// Defined below. A module only needs the type declared to take it as a
// reference parameter, which is all on_registered() asks for.
class ModuleRegistry;

// A native Resiris module (C++ port of the Python prototype modules).
class Module {
public:
    virtual ~Module() = default;

    // Every module states which kind it is. There is no default to forget,
    // because guessing wrong only shows up once a program is already running.
    virtual ModuleTarget target() const = 0;

    virtual std::string module_name() const = 0;
    virtual std::vector<std::string> function_names() const = 0;
    virtual std::map<std::string, std::string> variables() const = 0;

    // Plain function call, e.g. RSMath.sqrt(25). `args` are the user
    // arguments. Must throw ModuleSignatureError on signature mismatch.
    virtual Value call_function(const std::string& fn, const std::vector<Value>& args) = 0;

    // Object method call on a ModuleObject value. `handle` is the module-owned
    // object handle; `args` are the user arguments.
    virtual Value call_object_method(const std::string& fn, const std::shared_ptr<FrameAwareState>& handle,
                                     const std::vector<Value>& args) = 0;

    // Exported constant access. Throws ModuleError on unknown constant or
    // wrong exported type.
    virtual Value get_constant(const std::string& name) const = 0;

    // Called once by the registry right after every module has been collected,
    // so a module can see its siblings. It is not pure and has an empty default
    // because almost no module needs it: the registry itself is the only thing
    // that knows the full module list, and only a module like RSSystem, which
    // reports what is available, ever has to ask.
    //
    // This is also why modules must stay default-constructible. The build
    // generates the registry contents from the files in src/modules/, so it can
    // only ever call make_shared<Module>() and cannot pass arguments.
    virtual void on_registered(ModuleRegistry& registry) {
        (void)registry;
    }

    bool has_function(const std::string& fn) const {
        for (const auto& name : function_names()) {
            if (name == fn) {
                return true;
            }
        }
        return false;
    }
};

// The error code ModuleRegistry puts in its message when a host build refuses
// an ESP_ONLY module. The interpreter flattens every ModuleError into a plain
// ResirisError, so this marker is what lets a caller recognise the case and
// report the offending script name.
constexpr const char* kEspOnlyErrorCode = "EspOnlyOnHost";

// C++ port of the Python-side ModuleLoader. Modules are registered natively
// instead of loaded from Python files.
class ModuleRegistry {
public:
    // `host_build` is true on the PC target and false on the device. It only
    // decides whether an EspOnly module may be included, which is why a module
    // can name its kind absolutely instead of consulting a build flag.
    explicit ModuleRegistry(std::vector<std::shared_ptr<Module>> modules,
                            bool host_build = false)
        : host_build_(host_build) {
        for (auto& module : modules) {
            available_[module->module_name()] = std::move(module);
        }
        for (auto& entry : available_) {
            entry.second->on_registered(*this);
        }
    }

    // Every registered module name, in the map's own key order, so the list is
    // the same on every run. This includes modules the script never includes,
    // so it answers "what could I use", not "what is this program using".
    std::vector<std::string> module_names() const {
        std::vector<std::string> names;
        names.reserve(available_.size());
        for (const auto& entry : available_) {
            names.push_back(entry.first);
        }
        return names;
    }

    bool is_loaded(const std::string& module_name) const {
        return loaded_.count(module_name) != 0;
    }

    bool has_function(const std::string& module_name, const std::string& fn) const {
        if (loaded_.count(module_name) == 0) {
            return false;
        }
        auto found = available_.find(module_name);
        if (found == available_.end()) {
            return false;
        }
        return found->second->has_function(fn);
    }

    // <include> executes this. Duplicate includes are rejected, and a host
    // build rejects an EspOnly module here rather than letting the program run
    // with its hardware calls quietly missing.
    void load(const std::string& module_name) {
        if (loaded_.count(module_name) != 0) {
            throw ModuleError(
                module_name + " is already included. Error code:\"SameModuleMultiCall\"");
        }
        auto found = available_.find(module_name);
        if (found == available_.end()) {
            throw ModuleError(module_name + " not found! Error code:\"MissingModule\"");
        }
        if (host_build_ && found->second->target() == ModuleTarget::EspOnly) {
            throw ModuleError(module_name + " is ESP_ONLY. Error code:\"" +
                              kEspOnlyErrorCode + "\"");
        }
        loaded_.insert(module_name);
    }

    Value call_function(const std::string& module_name, const std::string& fn,
                        const std::vector<Value>& args) {
        Module& module = get(module_name);
        if (!module.has_function(fn)) {
            throw ModuleError(module_name + "." + fn + ": unknown module function");
        }
        validate_arguments(module_name, fn, args);
        Value result;
        try {
            result = module.call_function(fn, args);
        } catch (const ModuleSignatureError&) {
            throw ModuleError(module_name + "." + fn +
                              ": invalid argument count or module function arguments");
        }
        validate_result(module_name, fn, result);
        return wrap_module_result(module_name, result);
    }

    Value call_object_method(const std::string& module_name, const std::shared_ptr<FrameAwareState>& handle,
                             const std::string& method_name, const std::vector<Value>& args) {
        Module& module = get(module_name);
        if (!module.has_function(method_name)) {
            throw ModuleError(module_name + "." + method_name + ": unknown module function");
        }
        validate_arguments(module_name, method_name, args);
        Value result;
        try {
            result = module.call_object_method(method_name, handle, args);
        } catch (const ModuleSignatureError&) {
            throw ModuleError(module_name + "." + method_name +
                              ": invalid argument count or module function arguments");
        }
        validate_result(module_name, method_name, result);
        return wrap_module_result(module_name, result);
    }

    Value get_constant(const std::string& module_name, const std::string& constant_name) {
        Module& module = get(module_name);

        if (constant_name == "NAME") {
            return Value::make_string(module.module_name());
        }
        if (constant_name == "FUNCTIONS") {
            std::string joined;
            bool first = true;
            for (const auto& name : module.function_names()) {
                if (!first) {
                    joined += ", ";
                }
                joined += name;
                first = false;
            }
            return Value::make_string(joined);
        }
        if (constant_name == "VARIABLES") {
            std::string joined;
            bool first = true;
            for (const auto& [name, type] : module.variables()) {
                if (!first) {
                    joined += ", ";
                }
                joined += name + ":" + type;
                first = false;
            }
            return Value::make_string(joined);
        }

        auto var_types = module.variables();
        auto found_var = var_types.find(constant_name);
        if (found_var == var_types.end()) {
            throw ModuleError(module_name + "." + constant_name + ": unknown module constant");
        }

        Value value;
        try {
            value = module.get_constant(constant_name);
        } catch (const ModuleError& error) {
            throw;
        }
        std::string expected_type = found_var->second;
        std::string actual_type = python_type_name(value);
        if (expected_type != actual_type) {
            throw ModuleError(module_name + "." + constant_name + ": expected " +
                              expected_type + ", received " + actual_type);
        }
        return value;
    }

private:
    std::map<std::string, std::shared_ptr<Module>> available_;
    std::set<std::string> loaded_;
    bool host_build_ = false;

    Module& get(const std::string& module_name) const {
        if (loaded_.count(module_name) == 0) {
            throw ModuleError(module_name + ": module is not included");
        }
        auto found = available_.find(module_name);
        if (found == available_.end()) {
            throw ModuleError(module_name + ": module is not included");
        }
        return *found->second;
    }

    static void validate_arguments(const std::string& module_name, const std::string& fn,
                                   const std::vector<Value>& args) {
        (void)module_name;
        (void)fn;
        for (const auto& argument : args) {
            bool usable = false;
            switch (argument.type) {
                case Value::Type::Bool:
                case Value::Type::Int:
                case Value::Type::Float:
                case Value::Type::String:
                    usable = true;
                    break;
                default:
                    usable = false;
                    break;
            }
            if (!usable) {
                throw ModuleError(
                    value_repr(argument) +
                    " is not a usable modules argument! Error code:\"UnknownModuleArgument\"");
            }
        }
    }

    static void validate_result(const std::string& module_name, const std::string& fn,
                                const Value& result) {
        switch (result.type) {
            case Value::Type::Bool:
            case Value::Type::Int:
            case Value::Type::Float:
            case Value::Type::String:
            case Value::Type::ModuleObject:
                return;
            default:
                break;
        }
        throw ModuleError(module_name + "." + fn + ": module returned an unsupported value");
    }

    static Value wrap_module_result(const std::string& module_name, Value result) {
        if (result.type == Value::Type::ModuleObject) {
            if (!result.module_value) {
                result.module_value = std::make_shared<ModuleObject>(module_name, nullptr);
            } else if (result.module_value->module_name.empty()) {
                result.module_value->module_name = module_name;
            }
        }
        return result;
    }

    static std::string python_type_name(const Value& value) {
        switch (value.type) {
            case Value::Type::Bool: return "bool";
            case Value::Type::Int: return "int";
            case Value::Type::Float: return "float";
            case Value::Type::String: return "string";
            default:
                throw ModuleError("unsupported module value type: " + value.type_name());
        }
    }
};

} // namespace resiris