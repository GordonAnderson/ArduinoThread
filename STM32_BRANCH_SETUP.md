# ArduinoThread — `stm32` branch setup

Arduino-free port of Thread/ThreadController, mirroring the GAACE_Core `stm32`
branch. Same sources build on both the Arduino framework and bare STM32Cube.

## Files

**Modified** (overwrite in `src/`):

```
src/Thread.h
src/Thread.cpp
src/ThreadController.cpp
```

**New** (add to `src/`):

```
src/gthread_compat.h
```

**Also replace** (repo root):

```
library.json          frameworks -> "*", version -> 2.2.0-stm32
```

`ThreadController.h` is unchanged — it had no Arduino dependency beyond what
`Thread.h` pulls in.

## What changed

| Change | Detail |
|---|---|
| `#include <Arduino.h>` → `gthread_compat.h` | One line in `Thread.h` |
| `millis()` | 5 call sites, unchanged in source — the compat header maps it to `HAL_GetTick()` on the Cube path |
| `String Name` → `const char *Name` | Heap-free. **Borrowed pointer** — see below |
| `String ThreadName` → `char ThreadName[24]` | Under `USE_THREAD_NAMES`; `snprintf` replaces String concatenation. Length overridable via `THREAD_NAME_LEN` |
| `ThreadID = (int)this` → `(int)(uintptr_t)this` | Well-defined on 64-bit host test builds as well as 32-bit targets |
| `setName(NULL)` | Now stores `""` rather than NULL |
| `<stdio.h>`, `<stdint.h>`, `<string.h>` | Added where the Arduino umbrella header had been providing them implicitly |

### The one behavioural change to know about

`setName()` **stores the pointer, it does not copy.** The caller owns the
storage and must keep it alive for the life of the Thread.

```cpp
lockInThread.setName("lockin");        // fine - string literal, static storage

char buf[16];                          // WRONG - buf goes out of scope
snprintf(buf, sizeof buf, "ch%d", n);
t.setName(buf);
```

This affects Arduino builds too, which is why it lands on a branch rather than
straight on `main`. In practice every call site in the existing projects passes
a literal.

`getName()` never returns NULL — it returns `""` until `setName()` is called.
That matters because `ThreadController::get(const char*)` feeds it directly to
`strcmp()`.

## Tick source

Default on the Cube path is `HAL_GetTick()`, with the series HAL header selected
from the project's build flags (`STM32H743xx`, `STM32F407xx`, …).

Two override options:

**`GTHREAD_NO_HAL`** — skip the HAL include and forward-declare
`HAL_GetTick()`. Resolves at link time. Useful for host-side tests, and on real
targets where pulling the full HAL header into every translation unit is
unwelcome.

**`GTHREAD_MILLIS`** — supply any expression returning uint32_t milliseconds.

> **`GTHREAD_MILLIS` must be a global build flag, never a `#define` in one
> source file.** `Thread.cpp` and `ThreadController.cpp` are separate
> translation units and will not see a define made in `main.cpp` — they would
> silently fall back to `HAL_GetTick()` while the application used the
> override, giving two different clocks in one program. Same trap as the
> `ARDUINO` flag in the GAACE_Core port.

```ini
build_flags =
    -DGTHREAD_MILLIS=my_tick_source()
    -include my_tick.h
```

## Deliberately standalone

`gthread_compat.h` does **not** include `gaace_compat.h`. Thread is usable
without GAACE_Core and that independence is worth keeping; ten duplicated lines
is cheaper than the coupling.

`millis()` is declared `static inline`, so a translation unit including both
this and GAACE_Core's compat layer has no ODR conflict — each gets internal
linkage.

## Using it

```ini
lib_deps =
    https://github.com/GordonAnderson/GAACE_Core.git#stm32
    https://github.com/GordonAnderson/ArduinoThread.git#stm32
```

Application code is unchanged from the Arduino version:

```cpp
#include "ThreadController.h"

ThreadController controller(0);
Thread netThread(netTask, 1);
Thread heaterThread(heaterTask, 100);

void setup(void) {
    netThread.setName("net");
    heaterThread.setName("heater");
    controller.add(&netThread);
    controller.add(&heaterThread);
}

while (1) {
    controller.run();
}
```

## Verified

Compiles clean under `g++ -std=c++14 -Wall -Wextra` and passes an identical
behavioural test suite on **all three paths from the same sources**:

| Path | Flags |
|---|---|
| Pure Cube | `-DGTHREAD_NO_HAL` + user-supplied `HAL_GetTick()` |
| Arduino | `-DARDUINO=10819` with a stub `<Arduino.h>` |
| Custom tick | `-DGTHREAD_MILLIS='myTick()' -include mytick.h` |

Test coverage: interval accuracy over 1000 ms of simulated time (10 ms thread
ran 100×, 100 ms thread ran 10×), `getName()` non-NULL before `setName()`,
`get(name)` lookup including the miss case, `enabled` false suppressing
execution and re-enable resuming, `millis()` rollover forcing an immediate run,
and `setNextRunTime()` one-shot scheduling.

## Path back to `main`

Unlike the GAACE_Core port — which changed real behaviour on Arduino platforms
(String removal, `Stream*` → `GStream*` at ten sites) — this port is a header
swap, a tick-source macro, and two members changing type. Arduino behaviour is
unchanged apart from the `setName()` ownership rule.

Plan: branch, prove on the WeAct H743 bring-up board, re-test one Teensy or SAMD
project, then merge to `main` and retire the branch. Two long-lived ArduinoThread
branches are not worth maintaining for a change this small.
