# Resiris modules

This is the index for the five modules shipped with Resiris. Each module has
its own reference beside the native implementation:

| Module | Purpose | Reference |
|---|---|---|
| `RSMath` | Math operations not built into the language | [RSMath.md](modules/RSMath.md) |
| `RSBase` | Timer handles and timeout callbacks | [RSBase.md](modules/RSBase.md) |
| `RSSystem` | Runtime and ESP32 system information | [RSSystem.md](modules/RSSystem.md) |
| `RSConvert` | Text, number, encoding and binary conversions | [RSConvert.md](modules/RSConvert.md) |
| `RSSignal` | ESP32 GPIO and ADC access | [RSSignal.md](modules/RSSignal.md) |

Use the language reference for syntax and error behavior:
[HOW_TO_USE.md](HOW_TO_USE.md). To implement a native module in C++, see
[WRITING_MODULES.md](WRITING_MODULES.md).

## Including and calling modules

List each module once in an `<include>` statement. Separate multiple module
names with commas:

```resiris
<include> RSMath, RSBase

print_cmd(RSMath.sqrt(16.0))
v timer ModuleObject = RSBase.await()
```

Module names and function names are case-sensitive. Call module functions as
`Module.function(arguments)`, methods on returned handles as
`object.method(arguments)`, and read exported constants as `Module[CONSTANT]`.
Including an unknown module, including a module twice, calling an unavailable
function, or reading an unknown constant raises an error.

Every included module exposes these registry-provided constants:

| Form | Value |
|---|---|
| `Module[NAME]` | Module name |
| `Module[FUNCTIONS]` | Comma-separated function and method names |
| `Module[VARIABLES]` | Comma-separated exported `name:type` constants |

The PC runtime refuses modules marked ESP32-only when they are included. See
the module's own reference for its supported targets and host behavior.
