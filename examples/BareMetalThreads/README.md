# BareMetalThreads

ArduinoThread running on bare STM32Cube — no Arduino framework.

The scheduler code here is **identical** to the Arduino `BasicThreads` example.
That is the point of the port: `Thread` and `ThreadController` behave the same
on both platforms, and only the platform glue differs.

## What differs from the Arduino example

| | Arduino | Bare metal |
|---|---|---|
| Time source | `millis()` | `HAL_GetTick()`, mapped by `gthread_compat.h` |
| Entry point | `setup()` / `loop()` | `main()` with an explicit `while (1)` |
| Output | `Serial.print()` | `printf` retargeted through `__io_putchar()` |
| Scheduler API | — | **unchanged** |

## Why app.cpp and not main.c

Two problems with putting the scheduler in `main.c`:

1. `Thread` and `ThreadController` are **C++ classes**; CubeMX generates C.
2. CubeMX **rewrites `main.c`** on every code generation.

Converting `main.c` to `main.cpp` solves the first and makes the second worse.
So the application lives in `app.cpp`, reached through two C-linkage functions
declared in `app.h`. The only edits to `main.c` go inside CubeMX's USER CODE
markers, which regeneration preserves:

```c
/* USER CODE BEGIN Includes */
#include "app.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
app_setup();
/* USER CODE END 2 */

while (1)
{
  /* USER CODE BEGIN 3 */
  app_loop();
}
/* USER CODE END 3 */
```

That's the whole integration. This pattern generalises — it's how any C++
application layer should attach to a CubeMX project.

## Files

| File | Role |
|---|---|
| `src/app.h` | Two C entry points, plus the exact USER CODE insertions |
| `src/app.cpp` | Tasks, threads, and the main-loop body |
| `platformio.ini` | Build flags that matter for the library |

## What the project must supply

- `main.h` with HAL includes and pin macros
- `SystemClock_Config()`
- `MX_GPIO_Init()` defining `LED_GPIO_Port` / `LED_Pin`
- `MX_USART1_UART_Init()` and a `huart1` handle
- linker script and startup file

## What it demonstrates

**Three tasks at different rates**, plus a nested `ThreadController` for the
slow group — showing that controllers compose, since `ThreadController` extends
`Thread`.

**The run-to-completion rule.** Every task returns promptly. No `HAL_Delay()`,
no polling loops waiting on hardware. `readSensorTask()` shows the pattern for a
device with a long conversion time — a MAX31856 one-shot takes ~155 ms, so the
task triggers the conversion, returns, and reads the result on a later call.
Blocking there would stall every other task in the system.

**Runtime enable/disable to create a quiet window.** Setting `enabled = false`
on a group of threads suspends them without removing them from the controller.
For a measurement instrument this is how you get an interference-free interval:
no SPI traffic, no actuator switching, nothing on the bus while an acquisition
integrates. The scheduler becomes part of the noise strategy rather than
incidental to it.

**Per-task profiling** via `runTimeMs()`, which reports the wall-clock duration
of the last invocation — useful for finding the task that is quietly overrunning
its slice.

## Verified

`app.cpp` compiles clean under `g++ -Wall -Wextra` against HAL stubs and
produces the output above when driven through 3000 ms of simulated ticks.

## Notes

**Thread names are borrowed pointers on this branch.** `setName()` stores the
pointer rather than copying it, so the caller owns the storage. String literals
are safe; a stack buffer is not.

**Ordinary main-loop code coexists with the scheduler.** The periodic report in
this example is deliberately *not* a Thread, to make that clear.

**Time comparisons use subtraction**, `(int32_t)(now - due) >= 0`, not
`now >= due`. The subtraction form is correct across the 49-day tick rollover;
the comparison form stalls for ~49 days when the tick wraps.

**Interrupts are not affected.** The scheduler is cooperative, so anything with
hard real-time requirements belongs in an ISR, not in a task. A typical split
puts sample acquisition in a DMA-completion callback and everything else — the
network stack, control loops, sequencing, housekeeping — in threads.
