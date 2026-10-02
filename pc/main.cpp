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
#include "resiris/platform.hpp"


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
        << "Galena -- a Resiris interpreter futtatása galénán, ESP32 nélkül\n\n"
        << "Használat:\n"
        << "  " << program << " <program.resy>   egy .resy programot futtat\n"
        << "  " << program << " --embedded        azt a programot futtatja, amit\n"
        << "                          az eszköz jelenlegi buildje futtat\n"
        << "\n"
        << "Kapcsolók:\n"
        << "  --frames <N>     hány PROCESS() lifecycle keretet futtasson\n"
        << "                   (alapértelmezés 6, ugyanaz mint az ESP32-on)\n"
        << "  --main <útvonal> honnan olvassa a kiválasztott programot\n"
        << "                   --embedded esetén (alapértelmezés esp32/main.cpp)\n"
        << "  --version        verzió kiírása\n"
        << "  --help           ez a szöveg\n"
        << "\n"
        << "Kilépési kód: 0 siker, 1 a program kivételt dobott, 2 használati hiba.\n";
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
        if (argument == "--frames" || argument == "--main") {
            if (i + 1 >= argc) {
                std::cerr << "galena: " << argument << " utan kell egy ertek\n";
                return kUsageError;
            }
            const std::string value = argv[++i];
            if (argument == "--frames") {
                if (!parse_frames(value.c_str(), &options.process_frames)) {
                    std::cerr << "galena: ervenytelen keretszam: " << value << "\n";
                    return kUsageError;
                }
            }
            else {
                main_cpp = value;
            }
            continue;
        }
        if (!argument.empty() && argument[0] == '-') {
            std::cerr << "galena: ismeretlen kapcsolo: " << argument << "\n";
            return kUsageError;
        }
        if (!file.empty()) {
            std::cerr << "galena: csak egy program adható meg\n";
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
            std::cerr << "galena: nem sikerult kiolvasni a kiválasztott "
                         "programot innen: "
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
            std::cerr << "galena: ismeretlen program azonosito: " << identifier
                      << "\n  futtasd a scripts/generate_programs.py-t, hogy "
                         "frissuljon a header\n";
            return kUsageError;
        }
    }
    else {
        std::ifstream input(file, std::ios::binary);
        if (!input) {
            std::cerr << "galena: nem sikerult megnyitni: " << file << "\n";
            return kUsageError;
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        source = buffer.str();
        label = file;
    }

    options.label = label.c_str();
    return galena::run_program(source.c_str(), options) == 0 ? 0 : kProgramFailed;
}