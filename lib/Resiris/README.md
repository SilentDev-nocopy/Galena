# Resiris

Resiris is the programming language used by [Galena](../../README.md), a small
programmable computing device. The implementation is portable C++17 with no
dependencies, so the same source builds for a desktop host as well as for the
ESP32.

The language is deliberately small. The whole surface is seven types, a handful
of keywords and two built-in modules, and most of what you might expect to find
in a scripting language is not there. Knowing what is missing matters more than
what is there, so that comes first.

```resiris
c FPS float = 10.0

v x int = 10

START():
	print_cmd(x * 2)
```

## How a program is shaped

There is no `main`. A program is a flat list of statements, indentation forms
blocks, and the top level simply runs in source order. Functions and lifecycle
blocks are collected before anything else executes, so you can call a function
before you have written it.

```resiris
fn describe(n):
	mat n:
		1:
			return "one"
		2:
			return "two"
		else:
			return "many"

START():
	print_cmd(describe(1))
```

Two lifecycle blocks have a defined order. `START()` runs once. `PROCESS(FPS)`
runs at the rate of the global `c FPS float` constant. That pair is what most
Resiris programs are built around, because `PROCESS(FPS)` re-enters its body
fresh on every frame instead of nesting, which means a program can run forever
without its stack usage creeping up.

## What Resiris is for

Event-driven and periodic device automation: read an input, compute something,
drive an output, react to a timer.

```resiris
<include> RSBase

c FPS float = 4.0

v led ModuleObject = RSBase.await()
v ticks int = 0

START():
	led.set_timer(0.5)
	led.on_timeout("blink")

PROCESS(FPS):
	ticks += 1
	print_cmd(ticks)
	print_cmd(led.process())

fn blink():
	print_cmd("blink")
```

## What Resiris cannot do

There is no loop construct. No `while`, no `for`, no `break`, no `continue`.

There is no array or collection type either. The only types are scalars,
strings and closures, so there is nothing to iterate over even if there were a
loop.

That single constraint shapes the language more than anything else, and it is
worth being blunt about the consequence: iterating a collection, sorting,
searching, filtering and parsing text cannot be written in Resiris at all. Not
awkwardly — there is no way to express it. Scripts describe what the device
does; anything that needs to walk data lives in a module.

Recursion is the only general-purpose repetition left, and it is shallow. Each
level costs roughly 1.3 KB of native stack, so on an ESP32 with an 8 KB task
stack a script manages five or six levels before it runs out. That number is a
property of the host rather than a rule in the language, and nobody has measured
the exact ceiling, so treat five as the limit. If you find yourself counting
recursion, you have reached the edge of what the language is for.

## Design decisions that will surprise you

A few behaviours differ from what C, Python or Arduino would do. They are all
deliberate, and they are all easy to trip over.

### Integer division is an error

```resiris
print_cmd(7 / 2)      ## IntDivisionError
print_cmd(7.0 / 2)    ## 3.5
```

C would quietly hand you `3`. Resiris refuses instead, on the grounds that a
silently wrong answer in a calculation is worse than a crash. The error text
names `IntDivisionError` so you can search for it.

### Modulo follows Python

The result takes the sign of the divisor, not of the dividend.

| Expression | Resiris | C |
|---|---|---|
| `-7 % 3` | `2` | `-1` |
| `7 % -3` | `-2` | `1` |

### Indentation is tabs

Spaces in the indentation region are rejected outright, not converted and not
warned about. The message says so: `spaces cannot be used for indentation; use
tabs`.

### Numbers print like Python

Booleans print as `True` and `False`. A float with a whole value keeps its
`.0`, so you get `2.0` rather than `2`. Floats use the shortest representation
that round-trips, which means `0.1 + 0.2` prints as `0.30000000000000004`
instead of a tidied-up `0.3`. `int` is exact and 64 bits wide.

### `UnknownObject` fixes its type on first use

```resiris
v u UnknownObject = 42
print_cmd(u.type())   ## int
u = "a string now"    ## ResirisTypeError
```

### Name lookup has exactly two levels

A name resolves in the current function's frame, or in the globals. There is no
third step: a function called from inside another function cannot see its
caller's locals. This is the thing to understand before using a
`FunctionalObject`, because a closure can only reach its own parameters and the
globals.

```resiris
v counter FunctionalObject = FunctionalObject.new():
	index += 1     ## fine, `index` is a global
	return index

fn make():
	v hidden int = 1
	v increment FunctionalObject = FunctionalObject.new():
		hidden += 1   ## UnknownVariableError
		return hidden
	return increment
```

A `FunctionalObject` holds its parameters and its body; free names are looked up
in the scope stack at call time. Read it as a lightweight stateful handler over
globals rather than a proper lexical closure.

### Modules never grow the stack

All 29 `RSMath` functions are either an iterative loop or a single libm call, and
none of them calls back into the interpreter. A math call costs the same small
amount of stack whatever you pass it, so `factorial(10)` and `factorial(20)` are
identical in that respect. Math in a script can never be the thing that
exhausts the stack.

### Errors point at the source

Every error carries `line N, column M` and belongs to a distinct type, so a host
can tell a syntax error from a type error or a module error.

## The whole surface

| | |
|---|---|
| Types | `int` `float` `string` `bool` `ModuleObject` `FunctionalObject` `UnknownObject` |
| Declaration | `v` variable, `c` constant, `fn` function |
| Control flow | `if` / `elif` / `else`, `mat`, `return`, `pass` |
| Lifecycle | `START()`, `PROCESS(FPS)` |
| Output | `print_cmd(...)` |
| Operators | `+ - * / %`, `== != < <= > >=`, `+= -= *= /=`, member `.`, constant `[...]` |
| Built-ins | `str(x)`, `x.type()`, `x.type(T)`, `x.string()` |
| Modules | `RSMath`, `RSBase` |

Absent on purpose or for now: no `!` operator, no power operator, no bitwise
operators, no containers, no `null`, no comment syntax other than `##`.

Version `1.9-cpp`, in `resiris/version.hpp`.

## More

[HOW_TO_USE.md](HOW_TO_USE.md) is the full reference: lexical structure, every
type and statement, scoping rules, both module references, and the error model.

[WRITING_MODULES.md](WRITING_MODULES.md) covers the module interface for people
writing one in C++.

The Galena README covers how a Resiris program gets built and run.