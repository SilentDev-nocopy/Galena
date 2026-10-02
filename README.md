# Galena

> A standalone programmable computing environment built around the Resiris
> programming language.

Galena is an attempt to build a complete computing environment from the ground
up, with the hardware, system software, runtime and programming language
designed together. User programs never touch the ESP32 hardware directly.

## Where the project stands

The work so far is the language and its runtime. The tokenizer, parser and
interpreter run on the ESP32, along with two built-in modules. The rest of the
plan does not exist yet.

| Piece | State |
|---|---|
| Resiris tokenizer, parser, interpreter | implemented, runs on the device |
| `RSMath` module — 29 math functions, `PI`, `E` | implemented |
| `RSBase` module — 7 timer functions | implemented |
| Build-time embedding of `programs/*.resy` | implemented |
| Resiris compiler and bytecode | not started |
| Galena System — UI, display, input, storage | not started |
| Custom hardware | not started |

Programs are interpreted from source on the ESP32 at boot. There is no compiler
and no bytecode yet; moving to a bytecode VM is the planned direction.

## Building and running

```bash
pio run -t upload      # build and flash
pio device monitor     # 115200 baud
```

Before each build, `scripts/generate_programs.py` regenerates
`src/generated_programs.hpp` from `programs/*.resy` and asks which program the
build should run. Whichever answer you give, the script rewrites the single
`const char* source = ...;` line in `src/main.cpp`.

Two things follow from that. The build has to run from an interactive terminal,
since the script exits with an error on EOF. And `src/generated_programs.hpp`
together with that one line of `src/main.cpp` are build artifacts, so seeing them
modified in `git status` usually means a build picked a different program rather
than that the source changed.

Pass `--no-select` to regenerate the header without touching `main.cpp`.

## The language

Resiris is the language designed for this environment: small, and aimed at
event-driven and periodic device automation. Its syntax is Python-like, with
declaration keywords from C and lifecycle blocks from GDScript.

```resiris
c FPS float = 10.0

v x int = 10

fn bump(n):
	return n + 1

START():
	print_cmd(bump(x))

PROCESS(FPS):
	print_cmd("tick")
```

The constraint that shapes everything else: there is no loop construct and no
array type. Repetition comes from `PROCESS(FPS)`, which re-enters its block on
each frame without growing the interpreter's stack, or from recursion, which
reaches about five or six levels on an ESP32. That makes Resiris well suited to
describing what a device does, and unable to express iteration, so anything that
has to walk a collection, sort, search or parse belongs in a module.

- [lib/Resiris/README.md](lib/Resiris/README.md) — the language itself: what it
  is good at, what it cannot do, and the decisions that make it behave
  differently from C or Python.
- [lib/Resiris/HOW_TO_USE.md](lib/Resiris/HOW_TO_USE.md) — the full reference,
  covering every type and statement, scoping, both modules, and the error model.
- [lib/Resiris/WRITING_MODULES.md](lib/Resiris/WRITING_MODULES.md) — writing a
  module in C++.

The language is also maintained separately at
<https://github.com/SilentDev-nocopy/Resiris.git>.

### Programs in this repository

| File | Purpose |
|---|---|
| `programs/features.resy` | language self-test: types, operators, `mat`, closures, lifecycle, and 27 of the 29 `RSMath` functions |
| `programs/test.resy` | minimal smoke test |
| `programs/recursion_depth.resy` | recursion-depth regression test guarding the per-frame stack cost |

## Architecture

The intended stack, from user program down to hardware:

```text
┌─────────────────────────────────────┐
│              GALENA                 │
│                                     │
│  ┌───────────────────────────────┐  │
│  │       Resiris Programs        │  │
│  ├───────────────────────────────┤  │
│  │      Galena Runtime / VM      │  │
│  ├───────────────────────────────┤  │
│  │         Galena System         │  │
│  ├───────────────────────────────┤  │
│  │           Firmware            │  │
│  ├───────────────────────────────┤  │
│  │            Hardware           │  │
│  └───────────────────────────────┘  │
│                                     │
└─────────────────────────────────────┘
```

The top two layers are real. The runtime is an AST interpreter rather than the
bytecode VM planned for later, and the Galena System layer has not been started.
There is no designed firmware layer yet either; `src/main.cpp` is an Arduino
sketch that boots the interpreter and runs one embedded program, which is
scaffolding rather than the intended system. Hardware is a generic
ESP32-DevKitC for now.

The long-term flow is write, compile, transfer, run: compiled programs would be
transferred over USB or Wi-Fi, stored on the device and selected at runtime.
None of those steps exist yet.

## Repository layout

```text
Galena/
├── README.md
├── LICENSE
├── platformio.ini          # esp32dev / Arduino, C++17
├── scripts/
│   └── generate_programs.py # embeds programs/*.resy into a C++ header
├── src/
│   ├── main.cpp            # device entry point
│   └── generated_programs.hpp  # generated, do not edit
├── programs/               # the .resy programs that can be built and run
└── lib/
    └── Resiris/            # vendored copy of Resiris
        ├── README.md
        ├── HOW_TO_USE.md
        └── WRITING_MODULES.md
```

`lib/Resiris` is vendored directly into this repository rather than pulled in as
a submodule.

## Roadmap

```text
1. Language     → Resiris, compiler, runtime     ← current
2. System       → UI, input, display, storage
3. Applications → calculator, programs, games
4. Hardware     → ESP32-based Galena device
```

The hardware target is an ESP32-WROOM-32 with a 128×64 OLED display, a custom
keyboard, storage, a custom PCB and a custom enclosure. Treat those as
directions rather than committed specifications.

Still open: the compiled bytecode format and its file extension, the
architecture of the Galena System, and the final hardware.

## License

MIT, see [LICENSE](LICENSE).