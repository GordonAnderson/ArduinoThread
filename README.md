# ArduinoThread

Cooperative multitasking scheduler for Arduino — schedule periodic callbacks with configurable intervals, human-readable thread names, execution-time profiling, and `millis()` rollover protection.

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

---

## What "thread" means here

These are **not** OS threads. Each task is a function that runs to completion before returning control to the scheduler. There is no preemption, no per-task stack, and no `delay()` inside a task (that would block everything else). The benefit is minimal RAM overhead and straightforward, predictable execution on resource-constrained microcontrollers.

---

## Installation (PlatformIO)

Add to `platformio.ini`:

```ini
lib_deps =
    https://github.com/YOUR_ORG/ArduinoThread.git
```

Or reference a specific tag:

```ini
lib_deps =
    https://github.com/YOUR_ORG/ArduinoThread.git#v2.1.0
```

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

Thread names consume extra RAM. They are **disabled by default**. To enable, uncomment in `Thread.h`:

```cpp
#define USE_THREAD_NAMES 1
```

---

## Notes on `millis()` Rollover

`millis()` overflows back to zero after approximately 49.7 days. All timing values use `unsigned long` so subtraction-based comparisons wrap correctly. An additional guard in `shouldRun()` detects the case where the current time falls behind `last_run` and forces an immediate execution rather than a ~49-day stall.

---

## License

Original work by Ivan Seidel Gomes — released into the public domain.
Modifications by Gordon Anderson / GAA Custom Electronics, LLC — also released into the public domain.
