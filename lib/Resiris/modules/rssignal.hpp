#pragma once

#include <array>
#include <map>

#include "System/module.hpp"

namespace resiris {

// GPIO module for the ESP32-WROOM-32 pin map.
//
// The module drives and reads the real GPIO peripheral on the device. It is
// EspOnly: on a PC the registry refuses it at <include>, and the functions
// themselves throw rather than returning a value that describes no hardware.
//
// The pin map is the full set of GPIOs exposed by the WROOM-32 module. GPIO34,
// 35, 36 and 39 are input-only, so asking for them as outputs is an error
// instead of a quietly ignored request. Only the ADC1 pins are readable with
// get_analog_data: 32, 33, 34, 35, 36 and 39.
class RsSignalModule : public Module {
public:
    ModuleTarget target() const override { return ModuleTarget::EspOnly; }

    std::string module_name() const override { return "RSSignal"; }

    std::vector<std::string> function_names() const override {
        return {"set_signal_mode", "send_signal", "stop_signal",
                "get_signal_mode", "get_digital_data", "get_analog_data"};
    }

    std::map<std::string, std::string> variables() const override {
        return {
            {"BOOTSTRAP_GPIO0", "int"},
            {"UART0_GPIO1", "int"},
            {"BOOTSTRAP_GPIO2", "int"},
            {"UART0_GPIO3", "int"},
            {"GENERAL_DIGITAL_GPIO4", "int"},
            {"BOOTSTRAP_GPIO5", "int"},
            {"BOOTSTRAP_GPIO12", "int"},
            {"GENERAL_DIGITAL_GPIO13", "int"},
            {"GENERAL_DIGITAL_GPIO14", "int"},
            {"BOOTSTRAP_GPIO15", "int"},
            {"GENERAL_DIGITAL_GPIO16", "int"},
            {"GENERAL_DIGITAL_GPIO17", "int"},
            {"GENERAL_DIGITAL_GPIO18", "int"},
            {"GENERAL_DIGITAL_GPIO19", "int"},
            {"GENERAL_DIGITAL_GPIO21", "int"},
            {"GENERAL_DIGITAL_GPIO22", "int"},
            {"GENERAL_DIGITAL_GPIO23", "int"},
            {"GENERAL_DIGITAL_GPIO25", "int"},
            {"GENERAL_DIGITAL_GPIO26", "int"},
            {"GENERAL_DIGITAL_GPIO27", "int"},
            {"ADC1_WIFI_SAFE_GPIO32", "int"},
            {"ADC1_WIFI_SAFE_GPIO33", "int"},
            {"ADC1_WIFI_SAFE_GPIO34", "int"},
            {"ADC1_WIFI_SAFE_GPIO35", "int"},
            {"ADC1_WIFI_SAFE_GPIO36", "int"},
            {"ADC1_WIFI_SAFE_GPIO39", "int"},
        };
    }

    Value call_function(const std::string& fn, const std::vector<Value>& args) override;
    Value call_object_method(const std::string&, const std::shared_ptr<FrameAwareState>&,
                             const std::vector<Value>&) override {
        throw ModuleError("RSSignal has no object methods");
    }
    Value get_constant(const std::string& name) const override;

private:
    // The ESP32 has no way to read a pin's direction back, so the module keeps
    // the mode it configured. Indexed by GPIO number, which is 0..39.
    std::array<bool, 40> output_mode_{};
};

} // namespace resiris
