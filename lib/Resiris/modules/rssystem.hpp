#pragma once

#include <string>
#include <vector>

#include "System/module.hpp"

namespace resiris {

/*
 * Facts about the machine the program is running on: how much memory there is,
 * how fast the chip is, how large the flash is, and which modules this build
 * shipped.
 *
 * Every function returns one scalar or one string. There is nothing to iterate
 * over, which is what lets the module stay this small: a script prints a number
 * it asked for, and the module does the looking.
 *
 * Most of these facts belong to the ESP32 and do not exist on a desktop. On a
 * host they raise a ModuleError naming the function rather than returning a
 * stand-in, because a substituted 0 is exactly the kind of quietly wrong answer
 * this language refuses to produce elsewhere. The four that are genuinely
 * portable are cpu_load, cpu_cores, uptime and modules; see README.md for the
 * table.
 *
 * Two facts are about the running program rather than the machine, and the
 * module cannot read either of them itself, so the runtime installs them once in
 * platform.hpp: uptime, which counts from the moment the program started rather
 * than from boot, and cpu_load, the share of wall-clock time the interpreter
 * spent executing. The module reads them on demand instead of holding a copy, so
 * it stays default-constructible and can be registered like any other module.
 */
class RsSystemModule : public Module {
public:
    ModuleTarget target() const override { return ModuleTarget::EspAndPc; }

    std::string module_name() const override { return "RSSystem"; }

    std::vector<std::string> function_names() const override {
        return {"chip_model",   "chip_revision", "cpu_cores",
                "cpu_speed",    "cpu_load",     "memory_total",
                "memory_free",  "memory_used",  "memory_min_free",
                "psram_size",   "flash_total",  "flash_used",
                "uptime",       "modules",      "report"};
    }

    std::map<std::string, std::string> variables() const override { return {}; }

    Value call_function(const std::string& fn, const std::vector<Value>& args) override;

    Value call_object_method(const std::string& fn,
                             const std::shared_ptr<FrameAwareState>& handle,
                             const std::vector<Value>& args) override;

    Value get_constant(const std::string& name) const override;

    // Takes the registry instead of a list of names, so RSSystem is itself
    // included in what it reports. There is nothing to override for a module
    // that does not care what else is registered.
    void on_registered(ModuleRegistry& registry) override;

private:
    // The comma-joined names modules() returns, matching how the registry
    // answers X[FUNCTIONS].
    std::string module_list() const;

    // The whole machine in one preformatted string, because a script has no way
    // to collect the individual facts and print them in a loop.
    std::string build_report() const;

    // Every registered module name, including this one. Null until the registry
    // is built, which is what makes modules() fail loudly on an unregistered
    // instance rather than report an empty list.
    const ModuleRegistry* registry_ = nullptr;
};

} // namespace resiris