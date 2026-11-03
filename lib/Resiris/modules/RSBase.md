# RSBase

`RSBase` creates stateful timer handles that can schedule a named Resiris
function to run after a timeout. It is available on PC and ESP32. Include it
with `<include> RSBase`. See the [module index](../MODULES.md) for the other
built-in modules.

## API

| Call | Arguments | Result |
|---|---|---|
| `RSBase.await()` | none | A `ModuleObject` timer handle |
| `timer.set_timer(seconds)` | one number | Timer handle; starts or replaces the countdown |
| `timer.on_timeout("function_name")` | one string | Timer handle; sets the callback name |
| `timer.reset_timer()` | none | Timer handle; restarts the previous duration from now |
| `timer.stop_timer()` | none | Timer handle; stops the countdown |
| `timer.free_timer()` | none | Timer handle; stops and marks the timer freed |
| `timer.process()` | none | `float` seconds remaining, or `0.0` if stopped or expired |

The handle methods return the same timer state wrapped in a `ModuleObject`,
except `process()`, which returns a `float`. The timer does not advance by
itself: call `process()` regularly, normally once per `PROCESS(FPS)` frame.
When the countdown expires, `process()` returns `0.0` and schedules the
callback at most once. The interpreter invokes the named function after the
object-method call completes. Set or reset the timer to allow another timeout.

`set_timer` records the current interpreter time and starts the requested
duration. `reset_timer` uses the last configured duration. `stop_timer` makes subsequent `process()` calls return `0.0`; resetting
restarts it. `free_timer` also stops and marks the state as freed, but the
returned handle still refers to that state. `set_timer()` or `reset_timer()`
can reactivate it; the module does not invalidate a handle after `free_timer()`.

The callback must name a Resiris function. The module does not validate the
name when `on_timeout` is called; an invalid callback name fails when the
interpreter tries to invoke it.

## Example

```resiris
<include> RSBase

c FPS float = 10.0
v timer ModuleObject = RSBase.await()

START():
	timer.set_timer(0.5)
	timer.on_timeout("timeout")

PROCESS(FPS):
	print_cmd(timer.process())

fn timeout():
	print_cmd("timer expired")
```
