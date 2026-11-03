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
| [RSConvert](#rsconvert) | Text, number and encoding conversions |
| [RSSignal](#rssignal) | ESP32 GPIO outputs, digital inputs and ADC reads |

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

------

## RSConvert

Converts plain text and numbers into other formats: encodings, escapes, bases, bit patterns, colors, sizes and times. It only returns values; it creates no objects and changes nothing outside the program. Import with `<include> RSConvert`.

The module is `EspAndPc`, so it behaves identically on a PC and on the ESP32. Every text input and every decoded result must be ASCII (0..127); non-ASCII input raises an error rather than returning a mangled string. The module has no constants.

### Functions

There are 68 functions, in seven groups. Arguments are checked, and bad input raises `ResirisError` naming the function; nothing is silently truncated or guessed.

**Text encodings**

| Function | Arguments | Result |
| --- | --- | --- |
| `to_base64(text)` | ASCII text | Base64 with `=` padding |
| `from_base64(text)` | Base64 | ASCII text; rejects bad length or padding |
| `text_to_hex(text)` | ASCII text | two lowercase hex digits per character |
| `hex_to_text(text)` | hex digits | ASCII text; rejects odd length or non-hex |
| `integer_to_hex(number)` | non-negative `int` | lowercase hex, no `0x` |
| `hex_to_integer(text)` | hex digits | `int`; accepts `0x`/`0X` and upper or lower case |
| `url_encode(text)` | ASCII text | `%XX` escapes; leaves `A-Z a-z 0-9 - . _ ~` |
| `url_decode(text)` | escaped text | ASCII text; a space is `%20`, not `+` |
| `integer_to_binary(number)` | non-negative `int` | binary digits, no `0b` |
| `binary_to_integer(text)` | `0`/`1` | `int` |
| `integer_to_octal(number)` | non-negative `int` | octal digits, no `0o` |
| `octal_to_integer(text)` | `0`..`7` | `int` |
| `integer_to_base(number, base)` | non-negative `int`, base 2..36 | lowercase digits above 9 |
| `base_to_integer(text, base)` | text, base 2..36 | `int`; accepts upper or lower case |
| `text_to_binary(text)` | ASCII text | eight `0`/`1` digits per character |
| `binary_to_text(text)` | groups of eight bits | ASCII text |

**Escaping**

| Function | Arguments | Result |
| --- | --- | --- |
| `json_escape(text)` / `json_unescape(text)` | ASCII text | escapes quotes, `\`, and control characters; `\uXXXX` is decoded |
| `html_escape(text)` / `html_unescape(text)` | ASCII text | escapes `& < > " '` and their named entities |
| `xml_escape(text)` / `xml_unescape(text)` | ASCII text | the same five characters and predefined XML entities |
| `csv_escape(text)` / `csv_unescape(text)` | one field | quotes fields with `,`, `"` or a line break; doubles embedded quotes |
| `url_query_encode(text)` / `url_query_decode(text)` | ASCII text | query-component rules; a space becomes `+` |

**Characters and numbers**

| Function | Arguments | Result |
| --- | --- | --- |
| `ascii_to_integer(text)` | exactly one ASCII character | `int`, its code |
| `integer_to_ascii(number)` | `int` 0..127 | one-character `string` |
| `integer_to_roman(number)` | `int` 1..3999 | canonical Roman numerals |
| `roman_to_integer(text)` | Roman numerals | `int`; rejects non-canonical forms |

**Colors and time**

| Function | Arguments | Result |
| --- | --- | --- |
| `rgb_to_hex(red, green, blue)` | three `int` 0..255 | `"#rrggbb"`, lowercase |
| `hex_to_red(text)` / `hex_to_green(text)` / `hex_to_blue(text)` | `#rrggbb` or `rrggbb` | one `int` 0..255 |
| `seconds_to_duration(seconds)` | non-negative `int` | `HH:MM:SS`; hours may exceed 23 |
| `duration_to_seconds(text)` | `HH:MM:SS` | `int`; minutes and seconds are 00..59 |

**Sizes**

| Function | Arguments | Result |
| --- | --- | --- |
| `bytes_to_decimal_size(bytes)` | non-negative `int` | SI string such as `"1.5 MB"`, powers of 1000 |
| `bytes_to_binary_size(bytes)` | non-negative `int` | IEC string such as `"1.5 MiB"`, powers of 1024 |
| `storage_size_to_bytes(text)` | size string | `int`; units `B`, `kB`..`EB`, `KiB`..`EiB`, and `bit` |
| `bytes_to_bits(bytes)` | non-negative `int` | `int` |
| `bits_to_bytes(bits)` | non-negative `int`, multiple of 8 | `int` |

`bytes_to_*_size` chooses the largest applicable unit, prints at most two decimals, and drops trailing zeroes. `storage_size_to_bytes` accepts a non-negative decimal number and one unit, and the result must be a whole number of bytes; ambiguous spellings such as `KB` are rejected.

**Binary helpers**

| Function | Arguments | Result |
| --- | --- | --- |
| `integer_to_bytes(number, byte_count, order)` | non-negative `int`, count 1..8, `"big"`/`"little"` | hex bytes, two digits each |
| `bytes_to_integer(hex_text, order)` | hex bytes, `"big"`/`"little"` | `int`; rejects empty, odd or invalid hex |
| `signed_to_unsigned(number, bit_width)` | signed `int`, width 1..63 | two's-complement unsigned value |
| `unsigned_to_signed(number, bit_width)` | non-negative `int`, width 1..63 | two's-complement signed value |
| `float_to_bits(number, bit_width)` | `float`, width 16, 32 or 64 | IEEE 754 bits as a `0`/`1` string |
| `bits_to_float(bits)` | 16, 32 or 64 bits | `float`; width is inferred from the length |
| `integer_to_gray(number, bit_width)` | non-negative `int`, width 1..63 | exactly `bit_width` Gray bits |
| `gray_to_integer(bits)` | 1..63 bits | `int` |
| `integer_to_bcd(number)` | non-negative `int` | four bits per decimal digit, most significant first |
| `bcd_to_integer(bits)` | a multiple of four bits | `int`; each nibble must be 0..9 |
| `integer_to_leb128(number)` | non-negative `int` | unsigned LEB128 as hex bytes |
| `leb128_to_integer(hex_bytes)` | LEB128 hex bytes | `int`; rejects overlong or non-minimal encodings |
| `signed_to_zigzag(number)` | `int` in the supported range | non-negative `int` |
| `zigzag_to_signed(number)` | non-negative `int` | signed `int` |
| `decimal_to_fixed(value, fractional_bits)` | number, bits 0..62 | rounded fixed-point `int` |
| `fixed_to_decimal(number, fractional_bits)` | `int`, bits 0..62 | `float` |
| `rgb888_to_rgb565(red, green, blue)` | three `int` 0..255 | packed `int` 0..65535 |
| `rgb565_to_red(color)` / `rgb565_to_green(color)` / `rgb565_to_blue(color)` | packed `int` | one `int` 0..255, bit-replicated |
| `pcm_sample_to_bytes(sample, bit_width, order)` | signed sample, width 8/16/24/32, `"big"`/`"little"` | hex bytes |
| `bytes_to_pcm_sample(hex_bytes, bit_width, order)` | hex bytes, width 8/16/24/32, order | signed `int` |
| `signed_to_offset_binary(sample, bit_width)` | signed sample, width 8/16/24/32 | unsigned code |
| `offset_binary_to_signed(code, bit_width)` | unsigned code, width 8/16/24/32 | signed `int` |
| `extract_bit_field(number, start_bit, width)` | non-negative `int` | selected bits, shifted down to bit 0 |
| `replace_bit_field(number, start_bit, width, value)` | non-negative `int`, `value` | new `int` with the field replaced |

Bit positions are numbered from zero at the least significant bit. `start_bit` is 0..62, `width` is 1..63, and the field must fit inside 63 bits. Hex byte strings always use two lowercase digits per byte, so arbitrary binary data stays inside the module's ASCII-only text policy.

**Newlines**

| Function | Arguments | Result |
| --- | --- | --- |
| `newline_normalize(text)` | ASCII text | CRLF and lone CR become LF; everything else is unchanged |

### Usage

```resiris
<include> RSConvert

print_cmd(RSConvert.to_base64("Resiris"))          ## UmVzaXJpcw==
print_cmd(RSConvert.from_base64("UmVzaXJpcw=="))   ## Resiris
print_cmd(RSConvert.integer_to_hex(255))           ## ff
print_cmd(RSConvert.hex_to_integer("0xFF"))        ## 255
print_cmd(RSConvert.integer_to_roman(2024))        ## MMXXIV
print_cmd(RSConvert.rgb_to_hex(255, 128, 0))       ## #ff8000
print_cmd(RSConvert.seconds_to_duration(3661))     ## 01:01:01
print_cmd(RSConvert.bytes_to_binary_size(1572864)) ## 1.5 MiB
print_cmd(RSConvert.float_to_bits(1.5, 32))        ## 00111111110000000000000000000000
print_cmd(RSConvert.integer_to_leb128(300))        ## ac02
```

### Notes

**Every failure is an error, never a partial answer.** A byte field that does not fit, a non-canonical Roman numeral, an odd-length hex string, a bad Base64 pad, a non-ASCII character and a LEB128 value above `INT64_MAX` all raise `ResirisError` naming the function. That is deliberate: a silently truncated conversion is a wrong answer that is hard to notice later.

**`programs/rsconvert_test.resy` checks all of this.** It runs on both targets, so `make test` exercises the module on a PC and the same program can be selected for the ESP32 build to confirm the device agrees.

------

## RSSignal

Drives and reads the ESP32-WROOM-32 GPIO pins: digital outputs, digital inputs, and the ADC1 analog inputs. Import with `<include> RSSignal`.

The module is `EspOnly`. On a PC the registry refuses it at `<include>`, and the functions themselves raise `ModuleError` if a native caller reaches them, because there is no GPIO peripheral to describe. It has six functions and 26 pin constants.

### Functions

| Function | Arguments | Result |
| --- | --- | --- |
| `set_signal_mode(pin, output)` | pin constant, `bool` | `true`; configures `OUTPUT` or `INPUT` |
| `send_signal(pin)` | pin constant | `true`; drives the output HIGH |
| `stop_signal(pin)` | pin constant | `true`; drives the output LOW |
| `get_signal_mode(pin)` | pin constant | `bool`, `true` for output, `false` for input |
| `get_digital_data(pin)` | pin constant | `bool`, `true` for HIGH, `false` for LOW |
| `get_analog_data(pin)` | ADC pin constant | `float`, 0..4095 on the default 12-bit ADC |

`send_signal` and `stop_signal` require the pin to have been configured as an output first; on an input they raise an error. `get_signal_mode` reports the mode the module last configured, because the ESP32 cannot read a pin's direction back from the hardware.

### Pin constants

The constants are the GPIO numbers, named after their role on the WROOM-32:

| Group | Constants |
| --- | --- |
| Bootstrap | `BOOTSTRAP_GPIO0`, `BOOTSTRAP_GPIO2`, `BOOTSTRAP_GPIO5`, `BOOTSTRAP_GPIO12`, `BOOTSTRAP_GPIO15` |
| UART0 | `UART0_GPIO1`, `UART0_GPIO3` |
| General digital | `GENERAL_DIGITAL_GPIO4`, `GENERAL_DIGITAL_GPIO13`, `GENERAL_DIGITAL_GPIO14`, `GENERAL_DIGITAL_GPIO16`, `GENERAL_DIGITAL_GPIO17`, `GENERAL_DIGITAL_GPIO18`, `GENERAL_DIGITAL_GPIO19`, `GENERAL_DIGITAL_GPIO21`, `GENERAL_DIGITAL_GPIO22`, `GENERAL_DIGITAL_GPIO23`, `GENERAL_DIGITAL_GPIO25`, `GENERAL_DIGITAL_GPIO26`, `GENERAL_DIGITAL_GPIO27` |
| ADC1 / Wi-Fi safe | `ADC1_WIFI_SAFE_GPIO32`, `ADC1_WIFI_SAFE_GPIO33`, `ADC1_WIFI_SAFE_GPIO34`, `ADC1_WIFI_SAFE_GPIO35`, `ADC1_WIFI_SAFE_GPIO36`, `ADC1_WIFI_SAFE_GPIO39` |

GPIO34, 35, 36 and 39 are **input-only**. Asking for one of them as an output raises an error instead of configuring something that cannot work. Only the six ADC1 pins can be read with `get_analog_data`; any other pin raises an error there.

### Usage

```resiris
<include> RSSignal

v led int = RSSignal[GENERAL_DIGITAL_GPIO4]
v button int = RSSignal[BOOTSTRAP_GPIO5]
v sensor int = RSSignal[ADC1_WIFI_SAFE_GPIO32]

RSSignal.set_signal_mode(led, true)     ## GPIO4 as an output
RSSignal.set_signal_mode(button, false) ## GPIO5 as an input

RSSignal.send_signal(led)               ## GPIO4 HIGH
print_cmd(RSSignal.get_digital_data(button))
RSSignal.stop_signal(led)               ## GPIO4 LOW

print_cmd(RSSignal.get_analog_data(sensor))
```

### Notes

**Inputs do not need a mode call to be read, but outputs do.** `get_digital_data` reads any known pin. A pin that has never been configured as an output stays an input, which is the ESP32's power-on state.

**A floating input has no predictable level.** `get_digital_data` on an unconnected pin returns whatever the pad happens to see. Only a wired test can assert a particular level, which is why the loopback programs below use a jumper.

**The ADC is 12-bit by default.** `get_analog_data` returns the raw `analogRead` value, 0..4095, as a `float`. It does not convert to volts.

### Hardware tests

The GPIO tests are ordinary programs in `programs/`, selected from the normal build menu. Select one, upload, and watch the serial monitor at 115200 baud.

| Program | Wiring | What it checks |
| --- | --- | --- |
| `programs/rssignal_test.resy` | none | constants, mode switching, output calls, digital type, ADC range |
| `programs/rssignal_loopback_test.resy` | GPIO4 ↔ GPIO5 | real output-to-input digital loopback |
| `programs/rssignal_adc_test.resy` | GPIO25 ↔ GPIO32 | a digital output driving an ADC input |
| `programs/rssignal_input_only_error.resy` | none | GPIO34 must be refused as an output |

Each positive test prints `PASS` or `FAIL` per check and ends with a failure count; a failing check stops the program with an error, so `failures: 0` plus `Resiris: OK` means the run passed. The negative test is supposed to stop with `RSSignal.set_signal_mode: GPIO34 is input-only`.