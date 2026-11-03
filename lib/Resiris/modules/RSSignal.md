# RSSignal

`RSSignal` configures and reads ESP32-WROOM-32 GPIO pins. It is marked
`EspOnly`, so a PC runtime rejects `<include> RSSignal`. It is not a simulator.
The module exposes six functions and 26 integer GPIO constants. See the
[module index](../MODULES.md) for the other built-in modules.

## Functions

| Function | Arguments | Result |
|---|---|---|
| `RSSignal.set_signal_mode(pin, output)` | GPIO number (`int`), `bool` | `true` after configuring input or output |
| `RSSignal.send_signal(pin)` | GPIO number (`int`) | `true` after driving an output HIGH |
| `RSSignal.stop_signal(pin)` | GPIO number (`int`) | `true` after driving an output LOW |
| `RSSignal.get_signal_mode(pin)` | GPIO number (`int`) | `bool`; last mode configured through this module |
| `RSSignal.get_digital_data(pin)` | GPIO number (`int`) | `bool`; current digital level |
| `RSSignal.get_analog_data(pin)` | ADC1 GPIO number (`int`) | `float`; raw 12-bit `analogRead` value, 0..4095 |

Pass a pin constant as the `pin` argument. The functions validate that the
number is one of the exported pins. `send_signal` and `stop_signal` require a
prior `set_signal_mode(pin, true)` call. GPIO34, GPIO35, GPIO36 and GPIO39 are
input-only and cannot be configured as outputs. `get_analog_data` accepts only
the six ADC1 pins listed below; its result is a raw ADC count, not a voltage.

`get_signal_mode` reports the mode last configured by this module; the ESP32
does not provide this module with a way to read the pad direction back. A
floating or unconnected input has no predictable digital level.

## Pin constants

Each constant's value is its GPIO number.

| GPIO group | Constants |
|---|---|
| Bootstrap | `BOOTSTRAP_GPIO0`, `BOOTSTRAP_GPIO2`, `BOOTSTRAP_GPIO5`, `BOOTSTRAP_GPIO12`, `BOOTSTRAP_GPIO15` |
| UART0 | `UART0_GPIO1`, `UART0_GPIO3` |
| General digital | `GENERAL_DIGITAL_GPIO4`, `GENERAL_DIGITAL_GPIO13`, `GENERAL_DIGITAL_GPIO14`, `GENERAL_DIGITAL_GPIO16`, `GENERAL_DIGITAL_GPIO17`, `GENERAL_DIGITAL_GPIO18`, `GENERAL_DIGITAL_GPIO19`, `GENERAL_DIGITAL_GPIO21`, `GENERAL_DIGITAL_GPIO22`, `GENERAL_DIGITAL_GPIO23`, `GENERAL_DIGITAL_GPIO25`, `GENERAL_DIGITAL_GPIO26`, `GENERAL_DIGITAL_GPIO27` |
| ADC1 / Wi-Fi safe | `ADC1_WIFI_SAFE_GPIO32`, `ADC1_WIFI_SAFE_GPIO33`, `ADC1_WIFI_SAFE_GPIO34`, `ADC1_WIFI_SAFE_GPIO35`, `ADC1_WIFI_SAFE_GPIO36`, `ADC1_WIFI_SAFE_GPIO39` |

Observe the ESP32 board's electrical limits when wiring external devices;
**NOT ALL GPIOs ARE 5 V TOLERANT!**

## Example

```resiris
<include> RSSignal

v led int = RSSignal[GENERAL_DIGITAL_GPIO4]
v button int = RSSignal[BOOTSTRAP_GPIO5]
v sensor int = RSSignal[ADC1_WIFI_SAFE_GPIO32]

RSSignal.set_signal_mode(led, true)
RSSignal.set_signal_mode(button, false)
RSSignal.send_signal(led)
print_cmd(RSSignal.get_digital_data(button))
RSSignal.stop_signal(led)
print_cmd(RSSignal.get_analog_data(sensor))
```

## Device tests

| Program | Wiring | Purpose |
|---|---|---|
| [`programs/rssignal_test.resy`](../../../programs/rssignal_test.resy) | none | Constants, mode changes, output calls, types and ADC range |
| [`programs/rssignal_loopback_test.resy`](../../../programs/rssignal_loopback_test.resy) | GPIO4 ↔ GPIO5 | Digital output-to-input loopback |
| [`programs/rssignal_adc_test.resy`](../../../programs/rssignal_adc_test.resy) | GPIO25 ↔ GPIO32 | Digital output-to-ADC loopback |
| [`programs/rssignal_input_only_error.resy`](../../../programs/rssignal_input_only_error.resy) | none | Confirms GPIO34 is rejected as an output |

Select a program from the normal firmware build menu and watch the serial
monitor at 115200 baud. Positive tests should finish with `failures: 0`; the
input-only test is expected to stop with an error.
