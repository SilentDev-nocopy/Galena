# Resiris modules

This file is the catalogue of what a script can call once it has included a module.

- To use a module, read its section here.
- To write a module, see [WRITING_MODULES.md](WRITING_MODULES.md).
- The language reference is [HOW_TO_USE.md](HOW_TO_USE.md).

| Module                                                       | What it does                                                 |
| ------------------------------------------------------------ | ------------------------------------------------------------ |
| [RSMath](#rsmath) | Math the language lacks: logarithms, trigonometry, integer maths, roots |
| [RSBase](#rsbase) | Timers, and the Resiris functions they call when they expire |
| [RSSystem](#rssystem) | Reports on the machine: memory, chip, flash, program uptime  |

To add a module, add a section here and a row to the table above. Each section starts with what the module does, then lists its functions, then shows how to use them. Nothing in `HOW_TO_USE.md` needs to change, because that file only documents the syntax for reaching a module (`<include>`, `Module.func(args)`, `Module[NAME]`), not any particular module.

------

## How modules are reached

Declare modules with `<include>`, separated by commas:

```resiris
<include> RSMath, RSBase
```

Call a function as `Module.func(args)` and read a constant as `Module[NAME]`. The name must match exactly: `RSMath.sqrt(4.0)` works, `math.sqrt(4.0)` does not. Including the same module twice is an error.

The registry checks the argument types and the returned value for you. The number of arguments is up to each module, and a module may refuse a wrong count with a readable error. An unknown function or constant raises `ModuleError`. So does calling a hardware-only function on a PC; [RSSystem](#rssystem) shows what that looks like in practice.

[WRITING_MODULES.md](WRITING_MODULES.md#what-the-registry-checks-and-what-it-leaves-to-you) describes exactly what the registry checks and what it leaves to the module.

Every module answers three constants without being asked:

| Constant            | Value                                         |
| ------------------- | --------------------------------------------- |
| `Module[NAME]`      | The module's own name                         |
| `Module[FUNCTIONS]` | Its function list, comma-separated            |
| `Module[VARIABLES]` | Its constants as `name:type`, comma-separated |

Modules are the extension point for anything the language does not provide, including iteration. The language has no loops and no containers, so a script cannot walk through anything by itself. Whatever needs to be walked lives in a module.

------

## RSMath

Math the language does not have: logarithms, trigonometry, integer maths, rounding and roots. Import with `<include> RSMath`.

### Functions

There are 29 functions, plus the constants `PI` and `E`.

| Group              | Functions                                                   |
| ------------------ | ----------------------------------------------------------- |
| Rounding and roots | `abs` `sqrt` `cbrt` `pow` `floor` `ceil` `round`            |
| Logarithms         | `ln` `log10`                                                |
| Trigonometry       | `sin` `cos` `tan` `asin` `acos` `atan` `hypot` `pythagoras` |
| Integer maths      | `factorial` `ncr` `npr` `gcd` `lcm` `mod`                   |
| Ranges             | `min` `max` `clamp`                                         |
| Float predicates   | `is_nan` `is_inf` `is_finite`                               |

### Usage

```resiris
print_cmd(RSMath.sqrt(16.0))      ## 4.0
print_cmd(RSMath.pow(2.0, 10.0))  ## 1024.0
print_cmd(RSMath.factorial(10))   ## 3628800
print_cmd(RSMath.ncr(5, 2))       ## 10
print_cmd(RSMath.gcd(12, 18))     ## 6
print_cmd(RSMath.clamp(5.5, 1.0, 3.0))
print_cmd(RSMath[PI])
```

Each of these functions is either an iterative loop or a single libm call, and none of them calls back into the interpreter. Therefore no `RSMath` call grows the stack, and none can exhaust it.

------

## RSBase

Timers, and the Resiris functions they call when they expire. Timers are the language's only source of deferred work. Import with `<include> RSBase`.

### Functions

There are seven functions.

| Function                | Purpose                                                  |
| ----------------------- | -------------------------------------------------------- |
| `RSBase.await()`        | Create a timer handle, returned as a `ModuleObject`      |
| `set_timer(seconds)`    | Arm the timer                                            |
| `on_timeout("fn_name")` | Name the Resiris function to call when the timer expires |
| `reset_timer()`         | Rearm the timer                                          |
| `stop_timer()`          | Pause the timer                                          |
| `free_timer()`          | Release the handle                                       |
| `process()`             | Poll the timer; reports whether it fired                 |

### Usage

`process()` is required: it advances the handle's clock, so a timer that is never polled never fires. Call it once per `PROCESS` block for every handle whose timers should keep running.

```resiris
v timer ModuleObject = RSBase.await()
timer.set_timer(0.35) ##or RsBase.await.timer.set_timer(0.35)
timer.on_timeout("timer_callback")

PROCESS(FPS):
	print_cmd(timer.process())

fn timer_callback():
	print_cmd("timer callback fired")
```

Before each object method call, the interpreter supplies the current time. After the call, it fires the named callback once. This is the only way any module calls back into Resiris, and it is a single non-recursive call, so a timer callback cannot leak stack no matter how often it fires.

------

## RSSystem

Reports on the machine the program is running on: how much memory there is, how fast the chip is, how large the flash is, and how busy the program is. Import with `<include> RSSystem`.

### Functions

There are fifteen functions. Each one takes no arguments and returns a single number or a single string, so there is nothing to iterate over.

Four work on every target:

| Function               | Result                                                       |
| ---------------------- | ------------------------------------------------------------ |
| `RSSystem.cpu_load()`  | `float`, percent of wall-clock time the interpreter spent executing, 0..100 |
| `RSSystem.cpu_cores()` | `int`, number of processor cores                             |
| `RSSystem.uptime()`    | `float`, seconds since the program started                   |
| `RSSystem.modules()`   | `string`, the names this build registered, comma-separated   |

The other eleven read the ESP32's own silicon and flash:

| Function                     | Result                                         |
| ---------------------------- | ---------------------------------------------- |
| `RSSystem.chip_model()`      | `string`, e.g. `"ESP32-D0WD-V3"`               |
| `RSSystem.chip_revision()`   | `int`                                          |
| `RSSystem.cpu_speed()`       | `int`, MHz                                     |
| `RSSystem.memory_total()`    | `int`, heap bytes                              |
| `RSSystem.memory_free()`     | `int`, free heap bytes                         |
| `RSSystem.memory_used()`     | `int`, heap bytes in use                       |
| `RSSystem.memory_min_free()` | `int`, the lowest free heap since boot         |
| `RSSystem.psram_size()`      | `int`, bytes; `0` on a board without PSRAM     |
| `RSSystem.flash_total()`     | `int`, the physical flash chip's size in bytes |
| `RSSystem.flash_used()`      | `int`, bytes reserved by the app partition     |
| `RSSystem.report()`          | `string`, all of the above, preformatted       |

### Usage

```resiris
<include> RSSystem

print_cmd(RSSystem.chip_model())
print_cmd(RSSystem.memory_free())
print_cmd(RSSystem.modules())
print_cmd(RSSystem.report())
```

### Notes

Read these three notes before relying on a number from this module.

**On a PC, the eleven hardware functions raise `ModuleError`**, and the error names the function that was refused. They do not return a stand-in value, because a substituted `0` would be the kind of quietly wrong answer this language refuses to produce anywhere else. The four portable functions and `report()` do work on a host; `report()` simply leaves out the rows it cannot fill in. `programs/rssystem.resy` uses only this portable subset, which is why `make test` runs it.

**`flash_used()` is a reserved size, not a count of used bytes.** No filesystem is partitioned on the device yet, so it reports the size of the partition the firmware runs from. `flash_total()` is the physical chip, which is larger: on a DevKitC the values are 4194304 and 1310720 respectively.

**`cpu_load()` measures the interpreter, and with today's entry points it stays near 100%.** It is the share of wall-clock time spent tokenizing, parsing and executing. It is not the chip's total load, which would also include Wi-Fi and the idle task. Neither entry point sleeps between frames today, so the program never yields, and the honest answer is "always busy". It will become a real duty cycle once an entry point paces frames the way `Interpreter::run_process_forever` does. The counter advances between frames, so a `PROCESS` block that reads it sees the frames before the current one.