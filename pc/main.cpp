// Host entry point: runs the same Resiris programs on a PC that the ESP32
// firmware runs on the device.
//
// It shares the galena::run_program function with esp32/main.cpp and compiles the
// same lib/Resiris sources. The only difference between the two targets is which
// text sink is installed: stdout here, Serial on the device. So the behaviour
// agreement is not a convention to maintain by hand, it falls out of the shape.

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "galena_runtime.hpp"
#include "generated_programs.hpp"
#include "System/platform.hpp"


namespace {

constexpr int kProgramFailed = 1;
constexpr int kUsageError = 2;

constexpr const char* kVersion = "1.9-cpp";
constexpr const char* kDefaultMainCpp = "esp32/main.cpp";

// Reads the `const char* source = <identifier>;` line out of esp32/main.cpp.
// That is the very line scripts/generate_programs.py rewrites when a device build
// picks a program, so --embedded cannot drift away from the firmware: it reads
// back the same line the firmware was built from.
std::string selected_identifier(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        return std::string();
    }

    const std::string marker = "const char* source =";
    std::string line;
    while (std::getline(input, line)) {
        const std::size_t at = line.find(marker);
        if (at == std::string::npos) {
            continue;
        }
        std::string value = line.substr(at + marker.size());
        const std::size_t end = value.find(';');
        if (end != std::string::npos) {
            value.erase(end);
        }
        const std::size_t first = value.find_first_not_of(" \t\r\n");
        const std::size_t last = value.find_last_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return std::string();
        }
        std::string identifier = value.substr(first, last - first + 1);
        // main.cpp may namespace-qualify the variable
        // (resiris_programs::x_resy) while the header lists it under its bare
        // name, so any qualifier is stripped here.
        const std::size_t scope = identifier.rfind("::");
        if (scope != std::string::npos) {
            identifier.erase(0, scope + 2);
        }
        return identifier;
    }
    return std::string();
}

void print_usage(const char* program) {
    std::cout
        << "Galena -- runs Resiris programs on a PC, without an ESP32\n\n"
        << "Usage:\n"
        << "  " << program << " <program.resy>   runs one .resy program\n"
        << "  " << program << " --embedded        runs the program that the\n"
        << "                          device's current build runs\n"
        << "\n"
        << "Options:\n"
        << "  --frames <N>     how many PROCESS() lifecycle frames to run\n"
        << "                   (default 6, the same as on the ESP32)\n"
        << "  --forever, -f    run PROCESS() forever, sleeping between frames\n"
        << "                   according to FPS\n"
        << "  --main <path>    where to read the selected program from\n"
        << "                   for --embedded (default esp32/main.cpp)\n"
        << "  --version        prints the version\n"
        << "  --help           prints this text\n"
        << "\n"
        << "Exit code: 0 success, 1 the program threw, 2 usage error.\n";
}

bool parse_frames(const char* text, int* out) {
    try {
        *out = std::stoi(text);
        return *out >= 0;
    }
    catch (const std::exception&) {
        return false;
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string file;
    std::string main_cpp = kDefaultMainCpp;
    bool embedded = false;
    galena::RunOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        if (argument == "--version") {
            std::cout << "Galena (Resiris " << kVersion << ")\n";
            return 0;
        }
        if (argument == "--embedded") {
            embedded = true;
            continue;
        }
        if (argument == "--frames" || argument == "--main" || argument == "--forever" || argument == "-f") {
            if (argument == "--forever" || argument == "-f") {
                options.forever = true;
                continue;
            }
            if (i + 1 >= argc) {
                std::cerr << "galena: " << argument << " needs a value\n";
                return kUsageError;
            }
            const std::string value = argv[++i];
            if (argument == "--frames") {
                if (!parse_frames(value.c_str(), &options.process_frames)) {
                    std::cerr << "galena: invalid frame count: " << value << "\n";
                    return kUsageError;
                }
            }
            else {
                main_cpp = value;
            }
            continue;
        }
        if (!argument.empty() && argument[0] == '-') {
            std::cerr << "galena: unknown option: " << argument << "\n";
            return kUsageError;
        }
        if (!file.empty()) {
            std::cerr << "galena: only one program may be given\n";
            return kUsageError;
        }
        file = argument;
    }

    if (file.empty() && !embedded) {
        print_usage(argv[0]);
        return kUsageError;
    }

    // The program source, plus what its banner line says.
    std::string source;
    std::string label;

    if (embedded) {
        const std::string identifier = selected_identifier(main_cpp);
        if (identifier.empty()) {
            std::cerr << "galena: could not read the selected "
                         "program from: "
                      << main_cpp << "\n";
            return kUsageError;
        }
        const resiris_programs::ResyProgram* selected =
            resiris_programs::resy_program_by_identifier(identifier.c_str());
        if (selected != nullptr) {
            source = selected->source;
            label = selected->file;
        }
        if (source.empty()) {
            std::cerr << "galena: unknown program identifier: " << identifier
                      << "\n  run scripts/generate_programs.py so the "
                         "header updates\n";
            return kUsageError;
        }
    }
    else {
        std::ifstream input(file, std::ios::binary);
        if (!input) {
            std::cerr << "galena: could not open: " << file << "\n";
            return kUsageError;
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        source = buffer.str();
        label = file;
    }

    options.label = label.c_str();
    // `label` already holds the .resy file name in both the embedded and the
    // file branch, so an ESP_ONLY module can be reported against this script.
    options.program_name = label.c_str();
    options.host_build = true;
    return galena::run_program(source.c_str(), options) == 0 ? 0 : kProgramFailed;
}