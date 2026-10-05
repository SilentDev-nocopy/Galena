#include "modules/rssystem.hpp"

#include <cstdio>
#include <string>
#include <thread>

#include "System/platform.hpp"

#if defined(ARDUINO)
#include <Arduino.h>
#include <esp_partition.h>
#endif

namespace resiris {

namespace {

/*
 * Everything this module can learn from the chip, in one read.
 *
 * The hardware half only exists on an ESP32 build, so the whole read sits behind
 * ARDUINO and `known` records whether it happened. A host leaves `known` false
 * and the hardware-only functions refuse, which keeps the refusal in one place
 * instead of one #if per function.
 *
 * `cores` is the exception: it is a real fact on a host too, so it is filled in
 * on both and never refused.
 */
struct DeviceFacts {
    bool known = false;
    std::string chip_model;
    unsigned chip_revision = 0;
    unsigned cores = 0;
    unsigned cpu_mhz = 0;
    unsigned long heap_total = 0;
    unsigned long heap_free = 0;
    unsigned long heap_min_free = 0;
    unsigned long psram = 0;
    unsigned long flash_total = 0;
    unsigned long flash_used = 0;
};

DeviceFacts read_device_facts() {
    DeviceFacts facts;

#if defined(ARDUINO)
    facts.chip_model = ESP.getChipModel();
    facts.chip_revision = ESP.getChipRevision();
    facts.cpu_mhz = ESP.getCpuFreqMHz();
    facts.heap_total = ESP.getHeapSize();
    facts.heap_free = ESP.getFreeHeap();
    facts.heap_min_free = ESP.getMinFreeHeap();
    facts.psram = ESP.getPsramSize();
    facts.flash_total = ESP.getFlashChipSize();

    // How much flash this build reserves for itself, which is the size of the
    // partition the firmware runs from. No filesystem is flashed yet, so this is
    // a reserved size and not a used-bytes count.
    const esp_partition_t* app = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    facts.flash_used = app == nullptr ? 0UL : static_cast<unsigned long>(app->size);

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    facts.cores = chip.cores;

    facts.known = true;
#endif

    if (facts.cores == 0) {
        // Permitted to answer 0 when the host does not know, and a core count of
        // zero is not worth reporting.
        facts.cores = std::thread::hardware_concurrency();
    }

    return facts;
}

// What every hardware-only function says on a host. Naming the function keeps the
// error usable, since a script that only reads portable facts never reaches it.
void device_only(const std::string& fn) {
    throw ModuleError("RSSystem." + fn +
                      ": reads the ESP32's own hardware, so it has no answer on a PC");
}

void require_count(const std::vector<Value>& args, std::size_t count) {
    if (args.size() != count) {
        throw ModuleSignatureError();
    }
}

// What a function needing a host-installed measurement says when there is none.
// Naming the function keeps the error usable, since a script that only reads
// portable facts never reaches it.
void not_measured(const std::string& fn) {
    throw ModuleError("RSSystem." + fn +
                      ": no host installed a program clock, so it has no answer");
}

// Rows of the report are separated by newlines but the last one carries none,
// because print_cmd() supplies the newline that ends it.
std::string append_row(const std::string& so_far, const char* label,
                       const std::string& text) {
    if (so_far.empty()) {
        return std::string(label) + "     " + text;
    }
    return so_far + "\n" + label + "     " + text;
}

} // namespace

void RsSystemModule::on_registered(ModuleRegistry& registry) {
    registry_ = &registry;
}

Value RsSystemModule::call_function(const std::string& fn,
                                    const std::vector<Value>& args) {
    require_count(args, 0);

    // The handful of facts that exist on every target. Handled before the device
    // read so a host never pays for it.
    if (fn == "cpu_load") {
        if (!has_program_clock()) {
            not_measured(fn);
        }
        return Value::make_float(program_cpu_load());
    }

    if (fn == "uptime") {
        if (!has_program_clock()) {
            not_measured(fn);
        }
        return Value::make_float(program_seconds());
    }

    if (fn == "modules") {
        return Value::make_string(module_list());
    }

    if (fn == "report") {
        if (!has_program_clock()) {
            not_measured(fn);
        }
        return Value::make_string(build_report());
    }

    const DeviceFacts facts = read_device_facts();

    if (fn == "cpu_cores") {
        if (facts.cores == 0) {
            throw ModuleError("RSSystem.cpu_cores: the host reported no core count");
        }
        return Value::make_int(static_cast<Int>(facts.cores));
    }

    if (!facts.known) {
        device_only(fn);
    }

    if (fn == "chip_model") return Value::make_string(facts.chip_model);
    if (fn == "chip_revision") return Value::make_int(static_cast<Int>(facts.chip_revision));
    if (fn == "cpu_speed") return Value::make_int(static_cast<Int>(facts.cpu_mhz));
    if (fn == "memory_total") return Value::make_int(static_cast<Int>(facts.heap_total));
    if (fn == "memory_free") return Value::make_int(static_cast<Int>(facts.heap_free));
    if (fn == "memory_used") {
        return Value::make_int(static_cast<Int>(facts.heap_total - facts.heap_free));
    }
    if (fn == "memory_min_free") return Value::make_int(static_cast<Int>(facts.heap_min_free));
    if (fn == "psram_size") return Value::make_int(static_cast<Int>(facts.psram));
    if (fn == "flash_total") return Value::make_int(static_cast<Int>(facts.flash_total));
    if (fn == "flash_used") return Value::make_int(static_cast<Int>(facts.flash_used));

    throw ModuleError("RSSystem." + fn + ": unknown module function");
}

Value RsSystemModule::call_object_method(
    const std::string& fn, const std::shared_ptr<FrameAwareState>& handle,
    const std::vector<Value>& args) {
    // RSSystem hands out no ModuleObject and keeps no state, so a method call can
    // only be a script holding an object that belongs to something else.
    (void)handle;
    (void)args;
    throw ModuleError("RSSystem." + fn + ": RSSystem has no objects, so it has no methods");
}

Value RsSystemModule::get_constant(const std::string& name) const {
    // NAME, FUNCTIONS and VARIABLES never reach here; the registry answers them.
    throw ModuleError("RSSystem." + name + ": RSSystem exports no constants");
}

std::string RsSystemModule::module_list() const {
    if (registry_ == nullptr) {
        throw ModuleError(
            "RSSystem.modules: this instance was never registered, so it cannot "
            "know what else exists");
    }
    std::string joined;
    for (const std::string& name : registry_->module_names()) {
        if (!joined.empty()) {
            joined += ", ";
        }
        joined += name;
    }
    return joined;
}

std::string RsSystemModule::build_report() const {
    const DeviceFacts facts = read_device_facts();
    char line[96];
    std::string text;

    if (facts.known) {
        std::snprintf(line, sizeof(line), "%s rev %u", facts.chip_model.c_str(),
                      facts.chip_revision);
        text = append_row(text, "chip", line);
    }

    std::snprintf(line, sizeof(line), "%u core%s", facts.cores,
                  facts.cores == 1 ? "" : "s");
    text = append_row(text, "cpu", line);

    std::snprintf(line, sizeof(line), "%.1f%% load", program_cpu_load());
    text = append_row(text, "", line);

    if (facts.known) {
        std::snprintf(line, sizeof(line), "%lu / %lu bytes used, %lu lowest free",
                      facts.heap_total - facts.heap_free, facts.heap_total,
                      facts.heap_min_free);
        text = append_row(text, "memory", line);

        std::snprintf(line, sizeof(line), "%lu bytes", facts.psram);
        text = append_row(text, "psram", line);

        std::snprintf(line, sizeof(line), "%lu / %lu bytes reserved", facts.flash_used,
                      facts.flash_total);
        text = append_row(text, "flash", line);
    }

    std::snprintf(line, sizeof(line), "%.2f s", program_seconds());
    text = append_row(text, "uptime", line);

    return append_row(text, "modules", module_list());
}

} // namespace resiris