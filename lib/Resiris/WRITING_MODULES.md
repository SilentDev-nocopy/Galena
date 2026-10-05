# Writing Resiris modules

How to write a native Resiris module in C++: the whole `Module` interface, what
the registry checks for you, and a complete example that does nothing except
show what a module looks like from the inside.

[MODULES.md](MODULES.md) covers using the modules that already ship with the
language.

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

Everything is split in two folders. `System/` is the language itself — it knows
nothing about any specific module — and `modules/` is one folder of product
features, where a new module goes.

| File | Role |
|---|---|
| `System/module.hpp` | The `Module` base class, `ModuleSignatureError`, `ModuleRegistry` |
| `System/value.hpp` | `Value`, `ModuleObject`, `FrameAwareState` |
| `System/platform.hpp` | Host services, including the program clock `RSSystem` reads |
| `modules/rsmath.hpp` / `modules/rsmath.cpp` | `RsMathModule`, the simplest real module |
| `modules/rsbase.hpp` / `modules/rsbase.cpp` | `RsBaseModule`, a stateful module with object handles |
| `modules/rssystem.hpp` / `modules/rssystem.cpp` | `RsSystemModule`, a module that reads the machine instead of the script |

Each header sits beside the `.cpp` that implements it, and both are reached from
the `lib/Resiris/` folder: `#include "System/module.hpp"`,
`#include "modules/rsmath.hpp"`.

A module is **two files in `modules/`** and nothing else.
`scripts/generate_modules.py` reads every header in that folder before each
build and writes the registry contents into `generated_modules.hpp`, so a new
module ships on the ESP32 and on a PC at the same time without any further edit.
See [Registering it](#registering-it) for what the folder has to look like.

`RSSystem` is the one to read for a module that needs something the language
cannot supply: facts about the host. Its two host-measured values — `uptime` and
`cpu_load` — are read from `platform.hpp` on demand rather than taken as
constructor arguments, and its ESP32 hardware reads sit behind one `#if
ARDUINO` so the file still compiles for a host.

## The interface

All six methods below are pure virtual, so you implement every one of them or
the code will not compile.

`target()` is the seventh, and it is not optional: every module states which
kind it is.

```cpp
class Module {
public:
    virtual ~Module() = default;

    // Which targets can run this module. Required, see "Which kind of module
    // am I?" below.
    virtual ModuleTarget target() const = 0;

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

    // Provided, and empty. Override only to see the other registered modules:
    // virtual void on_registered(ModuleRegistry& registry);
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

`on_registered()` is the one optional member, and it is not pure, so it does not
count among the seven you must write. The registry calls it on each module once
they have all been collected, which is the only moment a module can find out what
else exists. `RSSystem.modules()` is the only thing in this codebase that needs
it; see [Registering it](#registering-it).

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

## Which kind of module am I?

Every module answers one extra question: can a PC run it?

```cpp
// module.hpp
enum class ModuleTarget {
    EspOnly,   // needs real ESP32 hardware
    EspAndPc,  // portable, runs on both targets
};
```

All three shipped modules are `EspAndPc`, and that is the right answer for each:
`RSMath` and `RSBase` are pure computation, and `RSSystem` marks itself portable
while refusing only the eleven functions that read the ESP32. So the check is
still unexercised — see the note on the hook below before relying on it. The
answer is one line:

```cpp
ModuleTarget target() const override { return ModuleTarget::EspAndPc; }
```

There is deliberately no default. Picking the wrong kind only shows up once a
program is already running on the wrong target, so the compiler asks instead.

**What the answer buys you.** The check runs at `<include>`, not at the call
site, so a script that includes an `EspOnly` module on a PC stops immediately,
before it has produced any output:

```console
$ ./build/galena blinky.resy
ModuleError: blinky.resy contains an ESP_ONLY module! On PC it can't run!
```

The device runs every module regardless of the answer, so an `EspOnly` module is
the normal case for anything that drives hardware.

The registry is refused rather than partially applied on purpose. There is no
return value to substitute for a missing hardware call, and a substituted `0`
would let the script keep running and quietly take the wrong branch. A script
that cannot run correctly does not run.

**The one thing this does not fix.** The check is reached only if the module
*compiles* on a host. A module that includes `<Arduino.h>` breaks the PC build
before `target()` is ever consulted, which takes the whole `make` down rather
than refusing one script. So a module marked `EspOnly` still has to be written
portably: keep the hardware access behind a hook that the entry point installs,
the way `set_text_sink` is installed for text output.

There are two portable shapes, and `RSSystem` shows the second one. A hook is
right when the host has to supply something the module cannot compute: text
output, or the two measurements behind `uptime` and `cpu_load`. A
`#if defined(ARDUINO)` guard is right when the module reads a fixed piece of
hardware and nothing else — the ESP32's heap size, the flash chip's capacity. The
whole read then sits behind the guard and returns a flag saying whether it
happened, so the refusal lives in one place:

```cpp
struct DeviceFacts { bool known = false; /* ... */ };

DeviceFacts read_device_facts() {
    DeviceFacts facts;
#if defined(ARDUINO)
    facts.heap_total = ESP.getHeapSize();
    facts.known = true;
#endif
    return facts;
}
```

Guarding each function separately with `#if` works too and reads more plainly,
but it scatters the host/device decision through the dispatch. Either way the
rule is the same: the file must build with plain `g++`, because `make` compiles
every `lib/Resiris/**/*.cpp` on the host regardless of what `target()` says.

## A complete example

This module does nothing useful. Every function returns a fixed value. Its only
job is to show the shape of a real module: the declarations, the dispatch, the
signature checks, an exported constant, an object handle with real state, and
how failures are reported.

Save the two files as `tutorial.hpp` and `tutorial.cpp`.

### modules/tutorial.hpp

```cpp
#pragma once

#include "System/module.hpp"

namespace resiris {

/*
 * A module that does nothing except demonstrate the shape of a module.
 *
 * Scripts call it as Tutorial.ping(), Tutorial.answer(), Tutorial.echo(...),
 * Tutorial.new_handle(), and Tutorial[GREETING].
 */
class TutorialModule : public Module {
public:
    ModuleTarget target() const override { return ModuleTarget::EspAndPc; }

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

### modules/tutorial.cpp

```cpp
#include "modules/tutorial.hpp"

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

Save both files as `lib/Resiris/modules/tutorial.hpp` and
`lib/Resiris/modules/tutorial.cpp`, and the module is registered. There is
nothing else to do.

`scripts/generate_modules.py` runs before every build and writes the folder into
`lib/GalenaRuntime/src/generated_modules.hpp`:

```cpp
#include "modules/tutorial.hpp"

inline std::vector<std::shared_ptr<resiris::Module>> make_modules() {
    return std::vector<std::shared_ptr<resiris::Module>>{
        std::make_shared<resiris::RsBaseModule>(),
        std::make_shared<resiris::RsMathModule>(),
        std::make_shared<resiris::RsSystemModule>(),
        std::make_shared<resiris::TutorialModule>(),   // added
    };
}
```

`galena_runtime.cpp` hands that straight to the registry, so this is the one
list and there is no second place to update. Because the generator can only
write `make_shared<>()`, two things follow for the folder:

1. **The header must have `#pragma once` and exactly one class deriving from
   `Module`.** The generator reads that class name out of the file, so there is
   no naming convention to follow. A helper class in the same folder is ignored;
   a second `Module` subclass is a build error, because the generator cannot tell
   which one you meant.
2. **The class must be default-constructible.** If a module needs data from the
   host, it asks for it instead of taking an argument.

The registry calls `on_registered(ModuleRegistry&)` on every module once they are
all collected, which is how a module learns its siblings. `RSSystem.modules()`
uses it to report what the build shipped, and it is why RSSystem now includes
itself in that list without having to be appended by hand:

```cpp
void RsSystemModule::on_registered(ModuleRegistry& registry) {
    registry_ = &registry;
}
```

The override is optional — the base class has an empty one — so a module that
does not care what else exists says nothing. Host measurements are the other
kind of thing a module cannot construct, and those come from
`platform.hpp`, which the runtime fills in once per run:

```cpp
// platform.hpp, called by the module at the point of use
if (!has_program_clock()) {
    throw ModuleError("RSSystem.cpu_load: no host installed a program clock");
}
return Value::make_float(program_cpu_load());
```

The runtime installs the clock with `set_program_clock()` before the program
runs.

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