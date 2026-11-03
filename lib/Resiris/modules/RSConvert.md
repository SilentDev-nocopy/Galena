# RSConvert

`RSConvert` converts ASCII text and scalar numbers between encodings, escaped
forms, bases, binary representations, colors, sizes and time formats. It is
available on PC and ESP32. It exports no constants and creates no objects.

Include with `<include> RSConvert`, then call `RSConvert.function(...)`.
The module provides 68 functions. Every text input and decoded text result
must be ASCII (code points 0..127); arbitrary binary data is represented as
hex or bit strings. Invalid input raises an error rather than returning a
partial conversion. See the [module index](../MODULES.md) for the other
built-in modules.

For table entries marked `int`, the implementation accepts an `int` or `bool`
argument. Entries marked number accept `int`, `float`, or `bool`.

## Text encodings

| Function | Arguments | Result |
|---|---|---|
| `to_base64(text)` | ASCII text | Base64 with `=` padding |
| `from_base64(text)` | Base64 text | ASCII text; rejects invalid length or padding |
| `text_to_hex(text)` | ASCII text | Two lowercase hex digits per character |
| `hex_to_text(text)` | Hex digits | ASCII text; rejects odd length or non-hex digits |
| `integer_to_hex(number)` | Non-negative integer | Lowercase hexadecimal without `0x` |
| `hex_to_integer(text)` | Hex digits | Integer; accepts `0x`/`0X` and either case |
| `url_encode(text)` | ASCII text | Percent escapes; leaves `A-Z a-z 0-9 - . _ ~` unchanged |
| `url_decode(text)` | xxxxxxxxxx <include> RSBase​c FPS float = 10.0v timer ModuleObject = RSBase.await()​START():    timer.set_timer(0.5)    timer.on_timeout("timeout")​PROCESS(FPS):    print_cmd(timer.process())​fn timeout():    print_cmd("timer expired")resiris | ASCII text; spaces are encoded as `%20`, not `+` |
| `integer_to_binary(number)` | Non-negative integer | Binary digits without `0b` |
| `binary_to_integer(text)` | `0`/`1` digits | Integer |
| `integer_to_octal(number)` | Non-negative integer | Octal digits without `0o` |
| `octal_to_integer(text)` | `0`..`7` digits | Integer |
| `integer_to_base(number, base)` | Non-negative integer, base 2..36 | Digits in lowercase; values above 9 use letters |
| `base_to_integer(text, base)` | Text, base 2..36 | Integer; accepts uppercase or lowercase digits |
| `text_to_binary(text)` | ASCII text | Eight `0`/`1` digits per character |
| `binary_to_text(text)` | Groups of eight bits | ASCII text |

## Escaping

| Function | Arguments | Result |
|---|---|---|
| `json_escape(text)` | ASCII text | Escapes quotes, backslashes and control characters |
| `json_unescape(text)` | JSON-escaped text | Decodes JSON escapes, including `\uXXXX`; output must be ASCII |
| `html_escape(text)` / `html_unescape(text)` | ASCII text / escaped text | Handles `& < > " '` and supported named entities |
| `xml_escape(text)` / `xml_unescape(text)` | ASCII text / escaped text | Handles the five predefined XML entities |
| `csv_escape(text)` | One field | Quotes fields containing comma, quote or line break; doubles embedded quotes |
| `csv_unescape(text)` | One field | Decodes the module's quoted-field format |
| `url_query_encode(text)` | ASCII text | Query-component encoding; space becomes `+` |
| `url_query_decode(text)` | Query-component text | Decodes percent escapes and `+` as space |

## Characters, numbers, colors and time

| Function | Arguments | Result |
|---|---|---|
| `ascii_to_integer(text)` | Exactly one ASCII character | Its integer code |
| `integer_to_ascii(number)` | Integer 0..127 | One-character string |
| `integer_to_roman(number)` | Integer 1..3999 | Canonical Roman numeral |
| `roman_to_integer(text)` | Roman numeral | Integer; non-canonical forms are rejected |
| `rgb_to_hex(red, green, blue)` | Three integers 0..255 | Lowercase `#rrggbb` string |
| `hex_to_red(text)` / `hex_to_green(text)` / `hex_to_blue(text)` | `#rrggbb` or `rrggbb` | Integer 0..255 |
| `seconds_to_duration(seconds)` | Non-negative integer | `HH:MM:SS`; hours may exceed 23 |
| `duration_to_seconds(text)` | `HH:MM:SS` | Integer; minutes and seconds must be 00..59 |

## Sizes

| Function | Arguments | Result |
|---|---|---|
| `bytes_to_decimal_size(bytes)` | Non-negative integer | SI-formatted string, e.g. `"1.5 MB"`; powers of 1000 |
| `bytes_to_binary_size(bytes)` | Non-negative integer | IEC-formatted string, e.g. `"1.5 MiB"`; powers of 1024 |
| `storage_size_to_bytes(text)` | Size string | Integer bytes; accepts `B`, `kB`..`EB`, `KiB`..`EiB`, and `bit` |
| `bytes_to_bits(bytes)` | Non-negative integer | Integer bits |
| `bits_to_bytes(bits)` | Non-negative integer divisible by 8 | Integer bytes |

The formatted size functions choose the largest applicable unit, show at most
two decimal places and omit trailing zeroes. `storage_size_to_bytes` requires
the converted value to be a whole number of bytes; ambiguous unit spellings
such as `KB` are rejected.

## Binary helpers

| Function | Arguments | Result |
|---|---|---|
| `integer_to_bytes(number, byte_count, order)` | Non-negative integer, count 1..8, `"big"` or `"little"` | Hex bytes, two lowercase digits per byte |
| `bytes_to_integer(hex_text, order)` | Hex bytes, byte order | Integer; rejects empty, odd-length or invalid hex |
| `signed_to_unsigned(number, bit_width)` | Signed integer, width 1..63 | Two's-complement unsigned representation |
| `unsigned_to_signed(number, bit_width)` | Non-negative integer, width 1..63 | Two's-complement signed value |
| `float_to_bits(number, bit_width)` | Number, width 16, 32 or 64 | IEEE 754 bit string |
| `bits_to_float(bits)` | 16, 32 or 64 bits | IEEE 754 float; width inferred from length |
| `integer_to_gray(number, bit_width)` | Non-negative integer, width 1..63 | Gray-code string of exactly that width |
| `gray_to_integer(bits)` | 1..63 bits | Integer |
| `integer_to_bcd(number)` | Non-negative integer | Four bits per decimal digit, most significant first |
| `bcd_to_integer(bits)` | Bit string whose length is divisible by four | Integer; each nibble must be 0..9 |
| `integer_to_leb128(number)` | Non-negative integer | Unsigned LEB128 as hex bytes |
| `leb128_to_integer(hex_bytes)` | LEB128 hex bytes | Integer; rejects overlong/non-minimal encodings |
| `signed_to_zigzag(number)` | Signed integer in supported range | Non-negative integer |
| `zigzag_to_signed(number)` | Non-negative integer | Signed integer |
| `decimal_to_fixed(value, fractional_bits)` | Number, fractional bits 0..62 | Rounded fixed-point integer |
| `fixed_to_decimal(number, fractional_bits)` | Integer, fractional bits 0..62 | Float |
| `rgb888_to_rgb565(red, green, blue)` | Three integers 0..255 | Packed integer 0..65535 |
| `rgb565_to_red(color)` / `rgb565_to_green(color)` / `rgb565_to_blue(color)` | Packed RGB565 integer | Integer 0..255 with bit replication |
| `pcm_sample_to_bytes(sample, bit_width, order)` | Signed sample, width 8/16/24/32, byte order | Hex bytes |
| `bytes_to_pcm_sample(hex_bytes, bit_width, order)` | Hex bytes, width 8/16/24/32, byte order | Signed integer |
| `signed_to_offset_binary(sample, bit_width)` | Signed sample, width 8/16/24/32 | Unsigned code |
| `offset_binary_to_signed(code, bit_width)` | Unsigned code, width 8/16/24/32 | Signed integer |
| `extract_bit_field(number, start_bit, width)` | Non-negative integer, valid field | Selected bits shifted down to bit 0 |
| `replace_bit_field(number, start_bit, width, value)` | Non-negative integer, valid field, replacement value | New integer with the field replaced |

Bit positions start at zero for the least significant bit. Bit fields use
`start_bit` 0..62 and `width` 1..63, and must fit within 63 bits. Byte-order
arguments are exactly `"big"` or `"little"`. Hex byte strings use two
lowercase digits per byte.

## Newlines

| Function | Arguments | Result |
|---|---|---|
| `newline_normalize(text)` | ASCII text | Converts CRLF and lone CR to LF; leaves other characters unchanged |

## Example

```resiris
<include> RSConvert

print_cmd(RSConvert.to_base64("Resiris"))           ## UmVzaXJpcw==
print_cmd(RSConvert.hex_to_integer("0xFF"))         ## 255
print_cmd(RSConvert.integer_to_roman(2024))          ## MMXXIV
print_cmd(RSConvert.rgb_to_hex(255, 128, 0))         ## #ff8000
print_cmd(RSConvert.seconds_to_duration(3661))       ## 01:01:01
print_cmd(RSConvert.bytes_to_binary_size(1572864))  ## 1.5 MiB
print_cmd(RSConvert.integer_to_leb128(300))          ## ac02
```

`programs/rsconvert_test.resy` is the portable self-test for this module and
runs on both PC and ESP32.
