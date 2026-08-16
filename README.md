# ArduinoThread

Cooperative multitasking scheduler for **Arduino and bare-metal STM32Cube** — schedule periodic callbacks with configurable intervals, human-readable thread names, execution-time profiling, and `millis()` rollover protection.

> **You are on the `stm32` branch.** Same sources build on both frameworks. The
> library is heap-free here: the Arduino `String` members have been replaced.
> See [Dual-framework support](#dual-framework-support) for the one behavioural
> change, and `STM32_BRANCH_SETUP.md` for the porting notes.
>
> The Arduino-only version lives on `main`.

---

## Origin and Credits

This library is a maintained fork of **ArduinoThread** by **Ivan Seidel Gomes** (March 2013).
Original repository: [https://github.com/ivanseidel/ArduinoThread](https://github.com/ivanseidel/ArduinoThread)
Released into the public domain by the original author.

### Modifications by Gordon Anderson / [GAA Custom Electronics, LLC](https://gaacustom.com)

- Added `setName()` / `getName()` for human-readable thread identification
- Added `getID()` to retrieve the unique thread ID
- Added `getInterval()` to read the configured interval
- Added `setNextRunTime()` to manually schedule the next execution
- Added `runTimeMs()` for per-task execution-time profiling
- Fixed `_cached_next_run` to be measured from dispatch time rather than completion time, giving more consistent periods
- Added `millis()` rollover guard in `shouldRun()` for robust operation past the 49-day boundary
- Added `get(const char *name)` to `ThreadController` for name-based thread lookup
- Corrected off-by-one in `ThreadController::run()` loop exit condition
- Switched all time values to `unsigned long` to match `millis()` return type and ensure correct wraparound arithmetic
- Cleaned up comments and code style throughout

**On the `stm32` branch additionally:**

- Removed the Arduino `String` class — the library is now heap-free
- Added `gthread_compat.h`, mapping `millis()` to `HAL_GetTick()` on bare-metal builds
- Added the `BareMetalThreads` example showing the STM32Cube integration pattern

---

## What "thread" means here

These are **not** OS threads. Each task is a function that runs to completion before returning control to the scheduler. There is no preemption, no per-task stack, and no `delay()` inside a task (that would block everything else). The benefit is minimal RAM overhead and straightforward, predictable execution on resource-constrained microcontrollers.

---

## Installation (PlatformIO)

```ini
lib_deps =
    https://github.com/GordonAnderson/ArduinoThread.git#stm32
```

Works unchanged with `framework = arduino` or `framework = stm32cube`.

---

## Dual-framework support

The same `Thread` and `ThreadController` sources build on both frameworks. The
library needs exactly one thing from its host: a millisecond tick.

| | Arduino | Bare-metal STM32Cube |
|---|---|---|
| Tick source | `millis()` | `HAL_GetTick()` |
| Selected by | `ARDUINO` defined | `ARDUINO` not defined |
| Scheduler API | — | **unchanged** |

`gthread_compat.h` handles the switch. Nothing to configure in the common case;
the series HAL header is chosen from the project's build flags (`STM32H743xx`,
`STM32F407xx`, …).

### One behavioural change to know about

`setName()` **stores the pointer, it does not copy.** The caller owns the
storage and must keep it alive for the life of the Thread.

```cpp
t.setName("sensor");                   // fine - literal, static storage

char buf[16];                          // WRONG - buf goes out of scope
snprintf(buf, sizeof buf, "ch%d", n);
t.setName(buf);
```

`getName()` never returns NULL — it returns `""` until set, which matters
because `ThreadController::get(const char*)` passes it straight to `strcmp()`.

### Build options

| Flag | Effect |
|---|---|
| `GTHREAD_NO_HAL` | Forward-declare `HAL_GetTick()` instead of including the full HAL header. Resolves at link time |
| `GTHREAD_MILLIS=expr` | Use a different tick source entirely — an RTOS tick, a hardware timer, or a host-test stub |
| `USE_THREAD_NAMES` | Auto-generate `"Thread <id>"` names into a fixed buffer |
| `THREAD_NAME_LEN` | Size of that buffer (default 24) |

> **`GTHREAD_MILLIS` must be a global build flag**, never a `#define` in one
> source file. `Thread.cpp` and `ThreadController.cpp` are separate translation
> units and would silently fall back to `HAL_GetTick()` while the application
> used the override — two different clocks in one program.

### C++ classes in a CubeMX project

CubeMX generates `main.c` and rewrites it on every code generation. Put the
scheduler in an `app.cpp` reached through C-linkage entry points, and add the
calls inside the USER CODE markers so they survive regeneration. See
`examples/BareMetalThreads/` for the complete pattern.

## Installation (Arduino IDE)

Download the repository as a ZIP and use **Sketch → Include Library → Add .ZIP Library**.

---

## Quick Start

```cpp
#include <Thread.h>
#include <ThreadController.h>

// Scheduler — runs all registered threads each loop() iteration
ThreadController controller;

// Individual tasks
Thread sensorThread;
Thread displayThread;

void readSensor() {
    // ... read ADC, update global state, etc.
}

void updateDisplay() {
    // ... refresh LCD or serial output
}

void setup() {
    sensorThread.setName("sensor");
    sensorThread.onRun(readSensor);
    sensorThread.setInterval(50);   // every 50 ms

    displayThread.setName("display");
    displayThread.onRun(updateDisplay);
    displayThread.setInterval(250); // every 250 ms

    controller.add(&sensorThread);
    controller.add(&displayThread);
}

void loop() {
    controller.run();
}
```

---

## API Reference

### `Thread`

#### Construction

```cpp
Thread t;                          // default: no callback, 0 ms interval
Thread t(myCallback, 100);        // callback + 100 ms interval
```

#### Configuration

| Method | Description |
|---|---|
| `setName(const char *name)` | Assign a human-readable name |
| `onRun(void (*callback)(void))` | Register the callback to execute |
| `setInterval(unsigned long ms)` | Set the repeat interval in milliseconds |
| `setNextRunTime(unsigned long t)` | Directly set the next scheduled `millis()` timestamp |
| `enabled` | Set `false` to pause without removing from the controller |

#### Queries

| Method | Returns |
|---|---|
| `getName()` | `const char*` — the thread's name |
| `getID()` | `int` — unique ID (derived from object address) |
| `getInterval()` | `unsigned long` — current interval in ms |
| `runTimeMs()` | `unsigned long` — wall-clock duration of the last run |
| `shouldRun(unsigned long time = 0)` | `bool` — true if it's time to run and `enabled` is set |

#### Execution

```cpp
t.run();  // Execute callback and update timing. Called automatically by ThreadController.
```

> **Important when subclassing:** If you override `run()`, you **must** call `runned()` inside your override, otherwise the thread will run on every tick.

---

### `ThreadController`

Manages up to `MAX_THREADS` (default 15, set in `ThreadController.h`) `Thread` pointers.

```cpp
ThreadController controller;       // runs on every loop() call
ThreadController controller(10);   // runs at most every 10 ms
```

#### Thread management

| Method | Description |
|---|---|
| `add(Thread *t)` | Add a thread. Returns `false` if the pool is full. |
| `remove(Thread *t)` | Remove by pointer |
| `remove(int id)` | Remove by thread ID |
| `clear()` | Remove all threads |

#### Queries

| Method | Returns |
|---|---|
| `size(bool cached = true)` | Number of registered threads |
| `get(int index)` | Nth thread by insertion order, or `NULL` |
| `get(const char *name)` | First thread matching the name, or `NULL` |

---

## Nested Controllers

Because `ThreadController` extends `Thread`, controllers can be nested:

```cpp
ThreadController fastGroup;   // poll every 10 ms
ThreadController slowGroup;   // poll every 500 ms
ThreadController root;        // top-level, called from loop()

root.add(&fastGroup);
root.add(&slowGroup);
```

---

## Enabling Thread Names (debug builds)

Auto-generated `"Thread <id>"` names consume extra RAM and are **disabled by
default**. Enable with the `USE_THREAD_NAMES` build flag, or uncomment in
`Thread.h`:

```cpp
#define USE_THREAD_NAMES 1
```

On this branch the name is a fixed `char[THREAD_NAME_LEN]` buffer (default 24
bytes) rather than a heap-allocated `String`.

This is independent of `setName()` / `getName()`, which are always available
and cost one pointer.

---

## Notes on `millis()` Rollover

`millis()` overflows back to zero after approximately 49.7 days. All timing values use `unsigned long` so subtraction-based comparisons wrap correctly. An additional guard in `shouldRun()` detects the case where the current time falls behind `last_run` and forces an immediate execution rather than a ~49-day stall.

---

## Examples

| Example | Framework | Shows |
|---|---|---|
| `BasicThreads` | Arduino | Periodic tasks, `setup()`/`loop()` integration |
| `BareMetalThreads` | STM32Cube | Same scheduler on bare metal: `app.cpp` pattern for CubeMX projects, nested controllers, the run-to-completion rule and the state-machine workaround for slow devices, and runtime enable/disable to create an interference-free quiet window |

---

## License

Original work by Ivan Seidel Gomes — released into the public domain.
Modifications by Gordon Anderson / GAA Custom Electronics, LLC — also released into the public domain.
