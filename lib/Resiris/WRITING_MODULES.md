# Writing Resiris modules

How to write a native Resiris module in C++: the whole `Module` interface, what
the registry checks for you, and a complete example that does nothing except
show what a module looks like from the inside.

[HOW_TO_USE.md §11–13](HOW_TO_USE.md#11-rsmath) covers using the modules that
already ship with the language.

---

## Why modules exist

Resiris has no loops, no collections and no I/O beyond `print_cmd`. A script on
its own can compute, branch and print. Everything else has to come from the host
running it.

A module is that bridge. It is a C++ class, registered with the interpreter
before the program runs, that gives scripts a set of named functions and
constants:

```resiris
<include> RSMath

print_cmd(RSMath.sqrt(16.0))   ## 4.0
print_cmd(RSMath[PI])          ## 3.14159…
```

So the split is deliberate. Scripts describe what the device does; modules provide
the how. If something a script needs cannot be written in the language itself, it
becomes a module rather than a change to the language.

## Where the code lives

| File | Role |
|---|---|
| `resiris/module.hpp` | The `Module` base class, `ModuleSignatureError`, `ModuleRegistry` |
| `resiris/value.hpp` | `Value`, `ModuleObject`, `FrameAwareState` |
| `rsmath.hpp` / `rsmath.cpp` | `RsMathModule`, the simplest real module |
| `rsbase.hpp` / `rsbase.cpp` | `RsBaseModule`, a stateful module with object handles |
| `galena_runtime.cpp` | Registers the modules; both build targets call it, so a module added here is available on the ESP32 and on a PC alike |

There are two module implementations today. Both sit in the library root rather
than under `resiris/`, because they are product features rather than language
infrastructure.

## The interface

All six methods below are pure virtual, so you implement every one of them or
the code will not compile.

```cpp
class Module {
public:
    virtual ~Module() = default;

    // The name scripts use: <include> X  →  X.func()  and  X[NAME]
    virtual std::string module_name() const = 0;

    // Every callable name, for validation. Not an implementation.
    virtual std::vector<std::string> function_names() const = 0;

    // Exported constants: name → type name.
    virtual std::map<std::string, std::string> variables() const = 0;

    // A plain call, e.g. RSMath.sqrt(25.0)
    virtual Value call_function(const std::string& fn,
                                const std::vector<Value>& args) = 0;

    // A method call on a ModuleObject value, e.g. timer.process()
    virtual Value call_object_method(const std::string& fn,
                                     const std::shared_ptr<FrameAwareState>& handle,
                                     const std::vector<Value>& args) = 0;

    // Reading an exported constant, e.g. RSMath[PI]
    virtual Value get_constant(const std::string& name) const = 0;

    // Provided. Do not override:
    bool has_function(const std::string& fn) const;
};
```

`function_names()` deserves a note. It returns the list of valid names so the
registry can reject calls it does not recognise before they reach you; dispatch
happens in the `if` chain inside `call_function`, not here. The practical
consequence is that a name missing from this list is unreachable from a script
even if your code implements it perfectly.

Method names share the same list. `has_function` gates both `call_function` and
`call_object_method`, so every method name has to appear in `function_names()`
as well.

## What the registry checks, and what it leaves to you

Knowing the exact split saves a lot of debugging.

**Argument count is yours.** The registry has no idea how many arguments your
functions take. Check it and throw `ModuleSignatureError`, which gets converted
into a readable `ModuleError`:

```cpp
if (args.size() != 2) {
    throw ModuleSignatureError{};   // → "X.fn: invalid argument count or
}                                  //    module function arguments"
```

**Argument types are checked for you.** Each argument must be `bool`, `int`,
`float` or `string`. Anything else, including a `ModuleObject`, is rejected
before your code runs:

```
<ModuleObject> is not a usable modules argument! Error code:"UnknownModuleArgument"
```

One consequence worth remembering: you cannot pass an object handle to a
different module function as an argument. Put what you need inside the object's
own state and reach it with a method call.

**Result types are checked for you.** You may return `bool`, `int`, `float`,
`string` or `ModuleObject`. A `FunctionalObject` or an empty `Value` is a
`ModuleError`:

```
X.fn: module returned an unsupported value
```

This is why a module cannot hand a script a closure.

**Constant types are checked for you.** `variables()` maps each exported constant
to the type it must have, and the registry compares that against what you
return. The type names it compares are exactly `bool`, `int`, `float`, `string`:

```cpp
std::map<std::string, std::string> variables() const override {
    return {{"PI", "float"}, {"E", "float"}};   // "string", not "str"
}
```

`get_constant()` should throw `ModuleError` for a name it does not recognise
rather than returning a default.

**Three constants come for free.** The registry answers these itself and never
calls your `get_constant()`:

| Constant | Value |
|---|---|
| `X[NAME]` | the module's own name |
| `X[FUNCTIONS]` | the function list, comma-joined |
| `X[VARIABLES]` | the constants as `name:type`, comma-joined |

**Errors.** Throw `ModuleSignatureError` for an arity mismatch and it arrives as
a tidy argument error. Throw `ModuleError` with your own message and the script
author sees that text, prefixed with the line and column. Anything else
propagates as a generic runtime error, so prefer one of the two.

## A complete example

This module does nothing useful. Every function returns a fixed value. Its only
job is to show the shape of a real module: the declarations, the dispatch, the
signature checks, an exported constant, an object handle with real state, and
how failures are reported.

Save the two files as `tutorial.hpp` and `tutorial.cpp`.

### tutorial.hpp

```cpp
#pragma once

#include "resiris/module.hpp"

namespace resiris {

/*
 * A module that does nothing except demonstrate the shape of a module.
 *
 * Scripts call it as Tutorial.ping(), Tutorial.answer(), Tutorial.echo(...),
 * Tutorial.new_handle(), and Tutorial[GREETING].
 */
class TutorialModule : public Module {
public:
    std::string module_name() const override { return "Tutorial"; }

    // handle, touch and read are object METHODS, but they still belong here:
    // has_function() gates both kinds of call.
    std::vector<std::string> function_names() const override {
        return {"ping", "answer", "echo", "new_handle",
                "handle", "touch", "read"};
    }

    // Exported constants: name -> type name.
    std::map<std::string, std::string> variables() const override {
        return {{"GREETING", "string"}};
    }

    Value call_function(const std::string& fn, const std::vector<Value>& args) override;

    Value call_object_method(const std::string& fn,
                             const std::shared_ptr<FrameAwareState>& handle,
                             const std::vector<Value>& args) override;

    Value get_constant(const std::string& name) const override;
};

// State behind an object this module hands out. Deriving from FrameAwareState is
// what enrols the object in the frame clock and callback protocol: the
// interpreter writes `now` before every method call, then fires `fire_callback`
// once afterwards if the method set it.
struct TutorialState : public FrameAwareState {
    int touches = 0;
    double first_touch_at = -1.0;  // negative until the first touch()
};

}  // namespace resiris
```

### tutorial.cpp

```cpp
#include "tutorial.hpp"

namespace resiris {

Value TutorialModule::call_function(const std::string& fn,
                                    const std::vector<Value>& args) {
    // Dispatch on the name. The registry has already type-checked `args`;
    // the count is our responsibility.

    if (fn == "ping") {
        if (!args.empty()) {
            throw ModuleSignatureError{};
        }
        return Value::make_string("pong");
    }

    if (fn == "answer") {
        if (!args.empty()) {
            throw ModuleSignatureError{};
        }
        return Value::make_int(42);
    }

    if (fn == "echo") {
        // One argument of any permitted type, handed back untouched.
        if (args.size() != 1) {
            throw ModuleSignatureError{};
        }
        return args[0];
    }

    if (fn == "new_handle") {
        if (!args.empty()) {
            throw ModuleSignatureError{};
        }
        // Returning a ModuleObject attaches our state to the value. The
        // registry fills in module_name, so building it here is enough.
        auto state = std::make_shared<TutorialState>();
        return Value::make_module(
            std::make_shared<ModuleObject>("Tutorial", state));
    }

    // The name is in function_names() but has no implementation. Say so.
    throw ModuleError("Tutorial." + fn + ": not implemented");
}

Value TutorialModule::call_object_method(const std::string& fn,
                                         const std::shared_ptr<FrameAwareState>& handle,
                                         const std::vector<Value>& args) {
    // Reached as <object>.fn(...) on a ModuleObject value. `handle` is the
    // state that value was created with.
    auto state = std::dynamic_pointer_cast<TutorialState>(handle);
    if (!state) {
        throw ModuleError("Tutorial." + fn + ": the object handle is not valid");
    }

    if (fn == "touch") {
        if (!args.empty()) {
            throw ModuleSignatureError{};
        }
        // The interpreter wrote `now` immediately before this call, so stamping
        // it here is how a module measures time without knowing anything about
        // its host.
        if (state->first_touch_at < 0.0) {
            state->first_touch_at = state->now;
        }
        state->touches += 1;
        return Value::make_int(state->touches);
    }

    if (fn == "read") {
        if (!args.empty()) {
            throw ModuleSignatureError{};
        }
        if (state->first_touch_at < 0.0) {
            throw ModuleError("Tutorial.read: no timing reference yet; call touch() first");
        }
        return Value::make_float(state->now - state->first_touch_at);
    }

    throw ModuleError("Tutorial." + fn + ": not implemented");
}

Value TutorialModule::get_constant(const std::string& name) const {
    if (name == "GREETING") {
        // The registry already checked that this is a `string`.
        return Value::make_string("hello from Tutorial");
    }
    // NAME, FUNCTIONS and VARIABLES never get here; the registry answers them.
    throw ModuleError("Tutorial." + name + ": unknown module constant");
}

}  // namespace resiris
```

### Registering it

A module does nothing until the registry knows about it, in `lib/GalenaRuntime/src/galena_runtime.cpp`:

```cpp
#include "tutorial.hpp"

auto modules = std::vector<std::shared_ptr<resiris::Module>>{
    std::make_shared<resiris::RsBaseModule>(),
    std::make_shared<resiris::RsMathModule>(),
    std::make_shared<resiris::TutorialModule>(),   // added
};

auto registry = std::make_shared<resiris::ModuleRegistry>(std::move(modules));
```

### Using it from Resiris

```resiris
<include> Tutorial

START():
	print_cmd(Tutorial.ping())          ## pong
	print_cmd(Tutorial.answer())        ## 42
	print_cmd(Tutorial.echo("hi"))       ## hi
	print_cmd(Tutorial[GREETING])        ## hello from Tutorial
	print_cmd(Tutorial[NAME])            ## Tutorial
	print_cmd(Tutorial[FUNCTIONS])      ## ping, answer, echo, new_handle, …

	v obj ModuleObject = Tutorial.new_handle()
	print_cmd(obj.touch())              ## 1
	print_cmd(obj.touch())              ## 2
	print_cmd(obj.read())               ## frame-clock seconds since the first touch
```

What each part is there to show:

- `ping` and `answer` are the minimum: check the argument count, return
  something.
- `echo` accepts and returns a `Value` untouched.
- `new_handle`, `touch` and `read` are the object model. The module allocates
  state, hands back a `ModuleObject`, and later recognises its own state from a
  base-class pointer. `touch` stamps the interpreter-injected `now` and `read`
  reports elapsed frame time against it, which is the usual way a module does
  timing without touching its host.
- `GREETING` is a declared constant, type-checked by the registry.
- `NAME` and `FUNCTIONS` are the free constants, answered without any code in
  the module.
- Every `ModuleSignatureError` shows what a wrong argument count looks like from
  the inside.
- The three `ModuleError` messages show explicit failure instead of a quiet
  default, so a mistake in a script is visible rather than mysterious.

## The short version

1. Implement all six pure virtuals.
2. List every name in `function_names()`, including method names, or nothing is
   callable.
3. Check the argument count yourself and throw `ModuleSignatureError`.
4. Never assume an argument's type; only `bool`, `int`, `float` and `string` can
   arrive.
5. Return only `bool`, `int`, `float`, `string` or `ModuleObject`. Never a
   `FunctionalObject` or an empty `Value`.
6. Declare constants in `variables()` with `"string"`, not `"str"`.
7. Throw `ModuleError` with a specific message rather than failing quietly.
8. Keep recursion out of `call_function` and `call_object_method`. A module runs
   on a stack that already has interpreter frames underneath it, and deep C++
   recursion in a module is a reliable way to crash a device.
9. Do not depend on the script's scope, and do not assume the host is an ESP32.
   The same library builds for a desktop.

## Calling convention

| In Resiris | Reaches |
|---|---|
| `<include> X` | `ModuleRegistry::load`, which rejects duplicates and unknown names |
| `X.fn(args)` | `call_function` |
| `obj.fn(args)` on a `ModuleObject` | `call_object_method` |
| `X[NAME]` | `get_constant` |

By the time your code runs, the registry has confirmed the module was included,
confirmed the name exists, and type-checked every argument. After your code
returns, it type-checks the result.

The error codes it produces:

| Code | Meaning |
|---|---|
| `MissingModule` | `<include>` named a module that was never registered |
| `SameModuleMultiCall` | the same module was included twice |
| `UnknownModuleArgument` | an argument was not a `bool`, `int`, `float` or `string` |
| — | `module is not included`, when called without `<include>` |
| — | `unknown module function` |
| — | `unknown module constant` |
| — | `module returned an unsupported value` |
| — | `expected <T>, received <U>`, when a constant's declared type did not match |