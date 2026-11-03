# RSMath

`RSMath` provides scalar math functions and the `PI` and `E` constants. It is
available on both PC and ESP32. Include it with `<include> RSMath`; call
functions as `RSMath.name(...)` and constants as `RSMath[PI]`.
See the [module index](../MODULES.md) for the other built-in modules.

All functions take the exact number of arguments shown below. Numeric
arguments accept `int`, `float`, or `bool` (`true` is treated as 1 and `false`
as 0). Integer arguments accept `int` or `bool`. Invalid signatures raise a
module argument error.

## Constants

| Constant | Type | Value |
|---|---|---|
| `PI` | `float` | π, approximately 3.141592653589793 |
| `E` | `float` | e, approximately 2.718281828459045 |

## Functions

| Function | Arguments | Result and behavior |
|---|---|---|
| `abs(x)` | number | Absolute value; returns `float` for a float input, otherwise `int` |
| `sqrt(x)` | number | Square root as `float`; negative inputs raise a math-domain error |
| `cbrt(x)` | number | Cube root as `float` |
| `pow(x, y)` | two numbers | `x` raised to `y`, as `float` |
| `floor(x)` | number | Greatest integer not greater than `x`, as `int` |
| `ceil(x)` | number | Least integer not less than `x`, as `int` |
| `round(x)` | number | Nearest integer, ties to even, as `int` |
| `ln(x)` | number | Natural logarithm as `float`; `x` must be positive |
| `log10(x)` | number | Base-10 logarithm as `float` |
| `sin(x)` | number | Sine as `float`; angle is in radians |
| `cos(x)` | number | Cosine as `float`; angle is in radians |
| `tan(x)` | number | Tangent as `float`; angle is in radians |
| `asin(x)` | number | Inverse sine as `float`; result is in radians |
| `acos(x)` | number | Inverse cosine as `float`; result is in radians |
| `atan(x)` | number | Inverse tangent as `float`; result is in radians |
| `hypot(x, y)` | two numbers | Hypotenuse `sqrt(x*x + y*y)`, as `float` |
| `pythagoras(x, y)` | two numbers | Alias for `hypot(x, y)` |
| `factorial(n)` | integer | `n!` as `int`; negative values raise a math-domain error |
| `ncr(n, r)` | two integers | Number of combinations; negative values raise a math-domain error, and `r > n` returns 0 |
| `npr(n, r)` | two integers | Number of permutations; negative values raise a math-domain error, and `r > n` returns 0 |
| `gcd(a, b)` | two integers | Greatest common divisor as `int` |
| `lcm(a, b)` | two integers | Least common multiple as `int`; returns 0 if either input is 0 |
| `mod(a, b)` | two numbers | Modulo with Python-style divisor-sign behavior; integer inputs return `int`, otherwise `float`; zero divisor is an error |
| `min(a, b)` | two numbers | Smaller value; returns one of the original arguments |
| `max(a, b)` | two numbers | Larger value; returns one of the original arguments |
| `clamp(value, low, high)` | three numbers | Constrains the value to the inclusive bounds and returns one of the input values |
| `is_nan(x)` | number | `bool`, true if `x` is NaN |
| `is_inf(x)` | number | `bool`, true if `x` is positive or negative infinity |
| `is_finite(x)` | number | `bool`, true if `x` is neither NaN nor infinite |

Integer results use Resiris's signed 64-bit `int`. The implementation does not
perform overflow checking for factorial, combinations, permutations, or least
common multiples; keep inputs within the range whose result fits in `int`.
`clamp` expects `low <= high`.

## Example

```resiris
<include> RSMath

print_cmd(RSMath.sqrt(16.0))       ## 4.0
print_cmd(RSMath.pow(2.0, 10.0))   ## 1024.0
print_cmd(RSMath.round(2.5))       ## 2
print_cmd(RSMath.ncr(5, 2))        ## 10
print_cmd(RSMath.mod(-7, 3))       ## 2
print_cmd(RSMath[PI])
```
