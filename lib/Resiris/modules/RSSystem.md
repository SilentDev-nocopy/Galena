# RSSystem

`RSSystem` reports runtime facts and ESP32 hardware information. Include it
with `<include> RSSystem`. All functions take no arguments; the module exports
no user constants or object methods. See the [module index](../MODULES.md) for
the other built-in modules.

## Functions available on both targets

| Function | Result | Notes |
|---|---|---|
| `RSSystem.cpu_cores()` | `int` | ESP32 core count or the host's reported hardware concurrency |
| `RSSystem.cpu_load()` | `float` | Program load percentage supplied by the active runtime |
| `RSSystem.uptime()` | `float` | Seconds since the current program run started |
| `RSSystem.modules()` | `string` | Comma-separated names of all modules registered in this build, not only modules included by the script |
| `RSSystem.report()` | `string` | Formatted summary; includes hardware rows on ESP32 and omits them on PC |

`cpu_load()`, `uptime()`, and `report()` require the host runtime to have
installed a program clock. The PC and ESP32 runtimes in this repository do so.
`cpu_cores()` raises an error if the host cannot report a non-zero core count.

## ESP32-only functions

These read the device's actual ESP32-WROOM-32 hardware. They raise a
`ModuleError` on a PC rather than returning fabricated values.

| Function | Result |
|---|---|
| `RSSystem.chip_model()` | `string`, chip model name |
| `RSSystem.chip_revision()` | `int`, silicon revision |
| `RSSystem.cpu_speed()` | `int`, CPU frequency in MHz |
| `RSSystem.memory_total()` | `int`, total heap bytes |
| `RSSystem.memory_free()` | `int`, currently free heap bytes |
| `RSSystem.memory_used()` | `int`, total heap minus free heap |
| `RSSystem.memory_min_free()` | `int`, lowest free heap observed since boot |
| `RSSystem.psram_size()` | `int`, PSRAM bytes; 0 if there is no PSRAM |
| `RSSystem.flash_total()` | `int`, physical flash capacity in bytes |
| `RSSystem.flash_used()` | `int`, size of the application partition in bytes |

`flash_used()` reports the application partition's reserved capacity; it is
not a count of bytes currently occupied by firmware or filesystem data.

## Example

```resiris
<include> RSSystem

print_cmd(RSSystem.cpu_cores())
print_cmd(RSSystem.uptime())
print_cmd(RSSystem.modules())
print_cmd(RSSystem.report())
```

`programs/rssystem.resy` demonstrates the portable calls. The hardware-only
functions require an ESP32 target.
