# Galena

> A standalone programmable computing environment built around the Resiris programming language.

Galena is a custom programmable computing environment designed around its own programming language, **[Resiris](https://github.com/SilentDev-nocopy/Resiris.git)**.

The project aims to create a small, self-contained computing device where users can write programs, compile them on a computer, transfer them to the Galena device, and run them using the Galena Runtime.

Galena is not intended to be a general-purpose operating system or an Arduino-like development platform. It is a dedicated computing environment designed around its own language, runtime, system and hardware.

---

## What is Galena?

Galena is the complete environment in which Resiris programs are developed and executed. It consists of several layers, from the hardware up to the programs that run on it:

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

The first implementation of Galena is planned as a custom ESP32-based programmable scientific calculator.

---

## Resiris

**Resiris** is the programming language designed for Galena.

**Resiris repository:**
https://github.com/SilentDev-nocopy/Resiris.git

It is intended to have a simple, readable syntax inspired by languages such as Python, C and GDScript, while being designed specifically for the Galena environment.

Example:

```resiris
v x int = 10

start():
        print_cmd(x)
```



---

## How it works

The intended development and execution flow is:

```text
             DEVELOPMENT PC
                  │
                  ▼
          ┌───────────────┐
          │  main.resy    │
          │ Resiris code  │
          └───────┬───────┘
                  │
                  ▼
          ┌───────────────┐
          │    Resiris    │
          │    Compiler   │
          └───────┬───────┘
                  │
                  ▼
          ┌───────────────┐
          │   Resiris     │
          │    Bytecode   │
          └───────┬───────┘
                  │
             USB / Wi-Fi
                  │
                  ▼
              GALENA
          ┌───────────────┐
          │    Storage    │
          │               │
          │  Programs     │
          └───────┬───────┘
                  │
            User selects
              a program
                  │
                  ▼
          ┌───────────────┐
          │    Galena     │
          │    Runtime    │
          └───────┬───────┘
                  │
                  ▼
          ┌───────────────┐
          │ Galena System │
          └───────┬───────┘
                  │
                  ▼
             ESP32 Hardware
```

In simple terms:

1. A Resiris program is written on a computer.
2. The Resiris Compiler converts the source program into the format that can be executed on Galena.
3. The compiled program is transferred to the Galena device.
4. The program is stored on the device.
5. The user selects the program.
6. The Galena Runtime executes the compiled program.

> **Note:** The final compiled format and file extension are not yet defined.

---

## Why a custom language?

Galena is built around the idea that the programming language and the computing environment should be designed together.

Instead of exposing the underlying ESP32 hardware directly to user programs, Resiris provides a higher-level programming environment. This allows the language to be designed specifically for:

- calculations
- programmable applications
- games
- Galena modules
- interaction with the Galena system

The goal is to make programming on a small dedicated device simple while still allowing the system to grow over time.

---

## Architecture

Galena is made up of the following components:

| Component | Description |
|---|---|
| **Resiris** | The programming language used to write Galena programs. |
| **Resiris Compiler** | Converts Resiris source code (`.resy`) into a compiled format that can run on the Galena device. Intended to run on a development computer, not on the ESP32 itself. |
| **Galena Runtime** | Executes compiled Resiris programs on the device. Execution is planned around compiled bytecode rather than interpreting `.resy` source directly on the ESP32. |
| **Galena System** | Provides the environment programs run in: UI, input, display, storage, modules, program execution and calculator functionality. Architecture still under development. |
| **Hardware** | The physical device Galena runs on — see below. |

### Hardware

The first Galena hardware implementation is planned around an ESP32. The current hardware direction includes:

- ESP32-WROOM-32
- 128×64 OLED display (SSD1309 controller)
- custom keyboard
- storage
- custom PCB
- custom enclosure

Hardware specifications may change during development.

---

## Applications

The Galena environment is intended to support several types of programs:

- **Calculator** — the initial and primary application: a programmable scientific calculator.
- **Programs** — users can create their own Resiris programs and run them on Galena.
- **Games** — Galena is also intended to support games written in Resiris.
- **Modules** — provide functionality to Resiris programs without exposing the underlying hardware directly.

---

## Development status & roadmap

Galena is an **active development project**, currently focused on establishing the foundations of the Resiris language and its compiler/runtime architecture, ahead of the Galena system itself.

Development proceeds in stages, starting with PC-based prototypes and testing before moving functionality onto the ESP32:

```text
1. Language     → Resiris, Compiler, Runtime
2. System       → UI, Input, Display, Storage
3. Applications → Calculator, Programs, Games
4. Hardware     → ESP32-based Galena device
```

Many components of the final system are still under development and should not be considered implemented unless explicitly documented as such.

**Open questions:**
- Final compiled bytecode format and file extension
- Exact architecture of the Galena System
- Final hardware specifications

---

## Repository structure

The repository will contain the Galena platform itself. A planned structure is:

```text
Galena/
│
├── README.md
├── LICENSE
├── CHANGELOG.md
│
├── docs/
├── hardware/
├── firmware/
├── system/
├── runtime/
├── calculator/
├── modules/
├── programs/
└── games/
```

Directories will be added as the corresponding components are developed.

---

## Related projects

### Resiris

The programming language used by Galena.

**Repository:**
https://github.com/SilentDev-nocopy/Resiris.git

---

## Long-term vision

The long-term goal of Galena is to create a small, self-contained computing environment where the hardware, system software, runtime and programming language are designed to work together.

The intended experience is simple:

```text
Write → Compile → Transfer → Run → Create
```

Galena is an experiment in building a complete computing environment from the ground up — from the hardware to the programming language.
