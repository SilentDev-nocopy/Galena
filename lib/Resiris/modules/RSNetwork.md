# RSNetwork

`RSNetwork` provides ESP32 Wi-Fi control and a small HTML server. Include it
with `<include> RSNetwork`. It is marked `EspAndPc`: the server works on both
targets, while the Wi-Fi functions are ESP32 only and raise a `ModuleError` on
a PC rather than reporting a network that does not exist.

All functions take no arguments unless shown. The module exports three string
constants and no variables. See the [module index](../MODULES.md) for the other
built-in modules.

## Constants

| Constant | Value | Meaning |
|---|---|---|
| `RSNetwork[ROUTEMODE_CRE]` | `"CRE"` | Connect to an existing router, station mode |
| `RSNetwork[ROUTEMODE_SR]` | `"SR"` | Self router, access point mode |
| `RSNetwork[ROUTEMODE_MB]` | `"MB"` | Man in between, station + access point |

## Wi-Fi (ESP32 only)

`RSNetwork.wifi()` returns a `ModuleObject` handle. All calls to it return
handles onto the same Wi-Fi state, so a mode set through one handle is visible
to the others. On a PC the call itself throws:

```
RSNetwork.wifi: Wi-Fi is ESP32 only and unavailable on a PC
```

Handle methods:

| Method | Result | Notes |
|---|---|---|
| `get_all_networks()` | `string` | Visible SSIDs joined with `", "` |
| `connect(ssid, password)` | `bool` | Waits up to 10 s for the station link; `false` on failure |
| `set_route_mode(mode)` | `bool` | `CRE` → station, `SR` → access point, `MB` → both; other values throw |
| `get_route_mode()` | `string` | The last mode set (`"CRE"` initially) |
| `get_ip(ssid)` | `string` | Device IP for its own AP SSID or the connected SSID; otherwise throws |
| `get_ssid(ip)` | `string` | Own AP SSID or connected SSID for a matching IP; otherwise throws |
| `disconnect()` | `bool` | `true` if a station link was active |
| `set_route_info(ssid, password)` | `bool` | Starts the access point; only in `SR` mode, password at least 8 characters |
| `get_own_route_info()` | `string` | SSID plus password masked as `X***Y` |

`RSNetwork.create_wifi_data_process_handler()` returns a second handle whose
only method is `get_global_speed() -> float`. Create it once in `START()` and
read it in `PROCESS(FPS)`.

The ESP32 has no counter for internet throughput, so this value is a **measured
byte rate** of the traffic through this module's server sockets: bytes sent and
received since the previous `get_global_speed()` call divided by the frame-clock
seconds between those calls. It reports `0.0` before the first sample, when no
time has passed, or when no traffic moved. It is not an internet speed test and
not a value inferred from signal strength.

## HTML server (both targets)

`RSNetwork.html_server_handler()` returns a handle with these methods:

| Method | Result | Description |
|---|---|---|
| `create_server(port int)` | `bool` | Port 1–65535; a second call throws |
| `set_page_content(html string)` | `bool` | HTML served at `/` and `/index.html` |
| `set_client_script(js string)` | `bool` | JavaScript served at `/rsclient.js` |
| `start_server()` | `bool` | Binds and listens. Requires `create_server()` first |
| `process()` | `bool` | Sends the buffered response, then accepts at most one new request |
| `get_request_state()` | `bool` | `true` while the newest request is unread |
| `request_string_data()` | `string` | Newest request value |
| `request_int_data()` | `int` | Newest value parsed as an integer |
| `request_float_data()` | `float` | Newest value parsed as a float |
| `request_boolean_data()` | `bool` | `"true"`/`"1"` or `"false"`/`"0"`, case-insensitive |
| `respond_string_data(value string)` | `bool` | Buffers the response sent by the next `process()` |
| `respond_int_data(value int)` | `bool` | As above, integer text |
| `respond_float_data(value float)` | `bool` | As above, float text |
| `respond_boolean_data(value bool)` | `bool` | As above, `"true"` or `"false"` |

Rules:

- Browser calls take the form `/call/<name>?value=<text>`. Only the newest
  request is kept; earlier ones are dropped.
- The first `request_*_data()` call marks a request read. Repeated calls return
  the same value. `get_request_state()` does not mark it read.
- A `respond_*_data()` call without a current request throws
  `RSNetwork.respond_<type>_data: no request awaiting a response`. A later
  response overwrites the buffered one.
- The buffered response goes out on the next `process()`; without one the
  browser receives an empty body.
- Static paths are answered immediately and never become API requests.
  Unknown paths return `404 Not Found`.
- On a PC the server binds to `127.0.0.1`. On the device it binds to the
  Wi-Fi IP address, so connect or start the access point before
  `start_server()` if the browser must reach it.
- On the device `start_server()` also starts the Wi-Fi stack if no mode is
  active: with Wi-Fi off, `WiFiServer::begin()` would assert inside lwIP and
  reset the chip. It starts station mode without joining any network; call
  `wifi.set_route_mode()` first if the server must run in `SR` or `MB` mode.

## Example

```resiris
<include> RSNetwork

c FPS float = 10.0
v page ModuleObject = RSNetwork.html_server_handler()
c port_number int = 8080
c html_page string = "<button rs-call='add' rs-value='5'>Send</button>"

START():
	page.create_server(port_number)
	page.set_page_content(html_page)
	page.start_server()

PROCESS(FPS):
	page.process()
	if page.get_request_state():
		v value int = page.request_int_data()
		page.respond_string_data("ok")
```

The `rsclient.js` client script and the HTML attributes it uses (`rs-call`,
`rs-value-from`, `rs-target`) are described in the `RSNetwork` design spec in
the Resiris project. A complete page using all three is in
`programs/rsnetwork_webpage.resy`: it starts an access point called
`galena-demo` with password `galena123` and serves the page on port 8080.
`programs/rsnetwork_test.resy` exercises the Wi-Fi API on the device;
`programs/rsnetwork_server_test.resy` exercises the portable server on a PC.
