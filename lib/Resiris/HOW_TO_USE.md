# Resiris in practice

The full reference: lexical structure, types, statements, scoping, the error
model, and the syntax for reaching a module. The
[README](README.md) explains why the language is shaped the way it is,
[MODULES.md](MODULES.md) lists what each built-in module offers, and
`WRITING_MODULES.md` is for writing modules in C++. Read the README first if you
have not.

---

## 1. Program structure

A program is a flat list of statements. Indentation forms blocks. There are no
classes and no `main`; the top level is the entry point and runs in source
order.

```resiris
<include> RSMath, RSBase

c FPS float = 10.0          ## constant
v counter int = 0           ## variable

fn helper(n):
	return n + 1

START():
	print_cmd(helper(counter))

PROCESS(FPS):
	counter += 1
```

Function and lifecycle definitions are gathered before execution starts, so a
function may be called above its own definition. Everything else then runs in
order.

---

## 2. Lexical structure

Indentation is counted in tab characters, and a space anywhere in the indentation
is a syntax error:

```
line 4: spaces cannot be used for indentation; use tabs
```

A block ends when indentation returns to a previous level. Comments run from
`##` to the end of the line; a line whose first non-blank content is `##` is
skipped entirely and does not open a block. Newlines end statements, though an
expression may wrap inside brackets.

| Kind | Operators |
|---|---|
| Arithmetic | `+` `-` `*` `/` `%` |
| Comparison | `==` `!=` `<` `<=` `>` `>=` |
| Assignment | `=` `+=` `-=` `*=` `/=` |
| Access | `.` for calls, `[...]` for module constants |
| Grouping | `(` `)` `,` |

There is no `!` operator, no power operator and no bitwise operators. To negate a
condition, write the opposite comparison.

---

## 3. Types

| Type | Declaration | Notes |
|---|---|---|
| `int` | `v x int = 10` | Exact, 64 bits. `-7 % 3` is `2`. `int / int` is an error. |
| `float` | `v x float = 10.0` | IEEE-754 double. `7.0 / 2` is `3.5`. |
| `string` | `v s string = "hi"` | Immutable. `+` concatenates, `<` compares lexicographically. |
| `bool` | `v b bool = true` | `true` / `false`. Not a number; arithmetic on it is a type error. |
| `ModuleObject` | `v t ModuleObject = RSBase.await()` | A module-owned handle with methods. |
| `FunctionalObject` | `v f FunctionalObject = …` | A callable value. See §6 and §7. |
| `UnknownObject` | `v u UnknownObject = 42` | Type inferred from the first assignment, then fixed. |

`print_cmd` renders booleans as `True` and `False`, and floats with the shortest
representation that round-trips, keeping the `.0` on integral values.

### Conversion

Nothing converts implicitly. Do it yourself:

```resiris
v x int = 10
v f float = x.type(float)     ## to float
v s string = x.type(string)   ## to string
v i int = x.type(int)
v t string = x.type()         ## the current type, as a string
v a string = str(x)           ## same as x.type(string)
```

`x.string()` is another way to write `x.type(string)`. `bool` and `int` do not
convert to one another, and there is no truthiness test that yields `1` or `0`.

---

## 4. Declarations

```resiris
v x int = 10                  ## variable, may be reassigned
c LIMIT int = 10              ## constant, assignment is an error
v y int                       ## only legal with UnknownObject, or with a value already present
```

Reassigning a `c` raises `ConstantAssignmentError`. A declaration without an
initialiser needs the name to already hold a value, otherwise you get
`MissingValueError`.

Compound assignment works on any non-constant variable:

```resiris
dec += 5
dec -= 2
dec *= 2
dec /= 2.0
```

---

## 5. Control flow

### Conditionals

```resiris
if val.type() == "int":
	return "int value"
elif val.type() == "float":
	return "float value"
else:
	return "other"
```

### Multi-way match

`mat` dispatches on a value and may have an `else`. It works as an expression
inside `fn`, and as a statement on its own.

```resiris
fn describe(n):
	mat n:
		1:
			return "one"
		2:
			return "two"
		else:
			return "many"

mat x.type():
	int:
		print_cmd("mat says int")
	else:
		print_cmd("mat says other")
```

### Loops

There are none, in any form. `PROCESS(FPS)` and recursion are the only sources
of repetition; see §8 and §9.

`pass` does nothing and is useful for an otherwise empty block. `return` exits a
function, and a function that never returns one produces no value.

---

## 6. Functions

```resiris
fn add(a, b):
	return a + b

fn no_value():
	pass
```

The argument count is checked, and a mismatch raises `FunctionError`. A function
may call itself, subject to the depth limit in §9.

### FunctionalObject

A `FunctionalObject` is a callable value. It stores its parameters and its body;
free names are resolved through the scope stack when it is called.

```resiris
v counter FunctionalObject = FunctionalObject.new():
	index += 1
	return index

print_cmd(counter())   ## 1
print_cmd(counter())   ## 2
```

It is called like any function and can be passed around, but §7 limits what it
is able to reference.

---

## 7. Scoping

A name is looked up in exactly two places: the current function's frame, and
the globals. There is no walk up through enclosing frames, so a function called
from inside another function cannot see the caller's locals.

```resiris
fn make():
	v hidden int = 1
	v increment FunctionalObject = FunctionalObject.new():
		hidden += 1      ## UnknownVariableError
		return hidden
	return increment
```

In practice this means a `FunctionalObject` can only reach its own parameters
and the globals. Globals are the intended place to keep the state of a stateful
handler, which is exactly what `counter` does in §6.

Assignment resolves the name through those same two levels and modifies it in
place, so a function can change a global directly.

---

## 8. Lifecycle

```resiris
c FPS float = 10.0

START():
	print_cmd("runs once")

PROCESS(FPS):
	print_cmd("runs every frame")
```

`START()` runs once, before any frame. `PROCESS(FPS)` then runs at the rate of
the global `c FPS float`, which must be a `float` constant or `ResirisTypeError`
is raised. The two names are recognised; they are written in uppercase by
convention because they are lifecycle blocks rather than ordinary functions.

Each frame re-enters the block rather than nesting into it, so `PROCESS(FPS)`
costs the same on frame one and on frame one thousand. That is why a Resiris
program usually runs indefinitely without approaching the recursion limit.

The host decides how many frames to run. Galena calls `run_process_frames(n)` or
`run_process_forever()`.

---

## 9. Recursion

A function may call itself. Each level adds a fixed amount of native stack to the
interpreter's, and the interpreter is running on the device's task stack, so the
depth available is:

> About 5 or 6 levels on an ESP32 with an 8 KB task stack. Treat 5 as the
> working limit.

This is a property of the host, not a rule in the language. A host with a larger
stack will allow deeper nesting.

---

## 10. Built-in functions and methods

| Form | Meaning |
|---|---|
| `str(x)` | Convert to string |
| `x.type()` | Name of the current type, as a string |
| `x.type(T)` | Convert to `T` |
| `x.string()` | Another spelling of `x.type(string)` |
| `obj.method(args)` | Call a method on a `ModuleObject` |
| `Module.func(args)` | Call a module function |
| `Module[NAME]` | Read a module constant |

Which module you are calling, and what it offers, is in
[MODULES.md](MODULES.md); writing one is in
[WRITING_MODULES.md](WRITING_MODULES.md).

---

## 11. Error model

Errors are typed and carry `line N, column M` with a descriptive message.

| Error | Raised when |
|---|---|
| `ResirisSyntaxError` | Tokenizing or parsing fails |
| `IntDivisionError` | `int / int` is attempted |
| `UnknownVariableError` | A name cannot be resolved |
| `ResirisTypeError` | Types do not line up, or a conversion is not possible |
| `ConstantAssignmentError` | A `c` is reassigned |
| `MissingValueError` | A declaration needs a value and has none |
| `FunctionError` | Unknown function, wrong argument count, or a name clash |
| `ModuleError` | Unknown module function or constant, or bad module arguments |

Division or modulo by zero raises `IntDivisionError` or a plain `ResirisError`
with the text `modulo by zero`.

There is no `try`/`catch` in the language. A script that raises stops, and the
host decides what happens next.

---

## 12. What is not in the language

Listed here so nothing comes as a surprise:

- No loops, no `break`, no `continue`
- No arrays, lists, maps or any container type
- No `null`; `Undefined` exists internally but is not a declared type
- No `!` operator, no power operator, no bitwise operators
- No `try`/`catch`
- No user-defined types, classes or modules
- No importing your own files; only built-in modules