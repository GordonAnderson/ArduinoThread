/*
    app.cpp - ArduinoThread on bare STM32Cube (no Arduino framework)

    The scheduler code below is IDENTICAL to the Arduino BasicThreads example.
    That is the point of the port: Thread and ThreadController behave the same
    on both platforms, and only the platform glue differs.

    Lives in app.cpp rather than main.c for two reasons:
      1. Thread/ThreadController are C++ classes
      2. CubeMX rewrites main.c on every code generation

    See app.h for the two lines to add inside CubeMX's USER CODE markers.

    What this shows
    ---------------
      * three periodic tasks at different rates
      * a nested ThreadController for a slow group
      * the run-to-completion rule, and the state-machine pattern for a device
        with a long conversion time
      * enable/disable at runtime to create an interference-free quiet window
      * per-task execution-time profiling via runTimeMs()

    Expected output, 115200 8N1:

        [boot] BareMetalThreads - ArduinoThread on STM32Cube
        [boot] SYSCLK = 480000000 Hz
        [ 1000 ms] fast=100 slow=10 blink=2  (fast took 0 ms)
        [ 2000 ms] fast=200 slow=20 blink=4  (fast took 0 ms)
        [ 2000 ms] quiet window: fast+slow disabled for 250 ms
        [ 2250 ms] quiet window over. fast=200 slow=20

    GAA Custom Electronics, LLC
*/

#include "main.h"               /* CubeMX: HAL, handles, pin macros */
#include "app.h"
#include "ThreadController.h"
#include <stdio.h>

/* --------------------------------------------------------------------------
   Platform glue

   The library needs exactly one thing from the platform: a millisecond tick.
   gthread_compat.h maps millis() to HAL_GetTick() automatically, so there is
   nothing to write for timing.

   printf retargeting is the only real glue, and only because this example
   prints. A production application would route output through GAACE_Core's
   command processor instead.
   -------------------------------------------------------------------------- */

extern UART_HandleTypeDef huart1;

extern "C" int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/* --------------------------------------------------------------------------
   Application state
   -------------------------------------------------------------------------- */

static volatile uint32_t fastCount  = 0;
static volatile uint32_t slowCount  = 0;
static volatile uint32_t blinkCount = 0;

/* --------------------------------------------------------------------------
   Tasks

   Every task must RUN TO COMPLETION and return. No delay(), no HAL_Delay(),
   no polling loop waiting on hardware. A task that blocks stalls every other
   task in the system.

   Work that takes time is expressed as a state machine across several calls -
   see readSensorTask().
   -------------------------------------------------------------------------- */

static void fastTask(void)
{
    fastCount++;
    /* Time-critical periodic work: servicing a ring buffer, stepping a
       control loop, advancing a state machine. */
}

static void slowTask(void)
{
    slowCount++;
}

static void blinkTask(void)
{
    blinkCount++;
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

/*
    State-machine pattern for a device with a long conversion time.

    A MAX31856 one-shot conversion takes roughly 155 ms. Waiting for it inside
    a task would block the scheduler for that whole period. Instead the task
    triggers the conversion, returns immediately, and collects the result on a
    later invocation.

    Note the subtraction-based time comparison: (int32_t)(now - due) >= 0 is
    correct across the 49-day tick rollover, whereas now >= due is not.
*/
enum SensorState { SENSOR_IDLE, SENSOR_CONVERTING };
static SensorState sensorState   = SENSOR_IDLE;
static uint32_t    conversionDue = 0;

static void readSensorTask(void)
{
    switch (sensorState)
    {
        case SENSOR_IDLE:
            /* trigger_conversion(); */
            conversionDue = HAL_GetTick() + 155;
            sensorState   = SENSOR_CONVERTING;
            break;

        case SENSOR_CONVERTING:
            if ((int32_t)(HAL_GetTick() - conversionDue) >= 0)
            {
                /* value = read_result(); */
                sensorState = SENSOR_IDLE;
            }
            break;
    }
}

/* --------------------------------------------------------------------------
   Threads

   ThreadController extends Thread, so controllers nest.
   -------------------------------------------------------------------------- */

static ThreadController root(0);        /* runs every pass of the main loop */
static ThreadController slowGroup(100); /* nested, polled every 100 ms      */

static Thread fastThread  (fastTask,       10);
static Thread slowThread  (slowTask,      100);
static Thread blinkThread (blinkTask,     500);
static Thread sensorThread(readSensorTask, 20);

static uint32_t nextReport = 1000;
static uint32_t quietUntil = 0;
static bool     quietDone  = false;

/* --------------------------------------------------------------------------
   Entry points
   -------------------------------------------------------------------------- */

void app_setup(void)
{
    printf("\r\n[boot] BareMetalThreads - ArduinoThread on STM32Cube\r\n");
    printf("[boot] SYSCLK = %lu Hz\r\n",
           (unsigned long)HAL_RCC_GetSysClockFreq());

    /* Names are BORROWED POINTERS on this branch - the caller owns the
       storage. String literals have static storage duration, so these are
       safe. A stack buffer would not be. */
    fastThread.setName("fast");
    slowThread.setName("slow");
    blinkThread.setName("blink");
    sensorThread.setName("sensor");

    root.add(&fastThread);
    root.add(&blinkThread);
    root.add(&slowGroup);

    slowGroup.add(&slowThread);
    slowGroup.add(&sensorThread);
}

void app_loop(void)
{
    root.run();

    /* ----------------------------------------------------------------------
       Quiet window.

       Disabling threads is how a measurement instrument creates an
       interference-free interval: no SPI traffic, no actuator switching,
       nothing on the bus while an acquisition integrates. The scheduler
       becomes part of the noise strategy rather than incidental to it.
       ---------------------------------------------------------------------- */
    if (!quietDone && HAL_GetTick() >= 2000)
    {
        printf("[%5lu ms] quiet window: fast+slow disabled for 250 ms\r\n",
               (unsigned long)HAL_GetTick());
        fastThread.enabled = false;
        slowThread.enabled = false;
        quietUntil = HAL_GetTick() + 250;
        quietDone  = true;
    }

    if (quietUntil && (int32_t)(HAL_GetTick() - quietUntil) >= 0)
    {
        fastThread.enabled = true;
        slowThread.enabled = true;
        printf("[%5lu ms] quiet window over. fast=%lu slow=%lu\r\n",
               (unsigned long)HAL_GetTick(),
               (unsigned long)fastCount, (unsigned long)slowCount);
        quietUntil = 0;
    }

    /* Periodic report. Deliberately NOT a Thread, to show that ordinary
       main-loop code coexists with the scheduler. */
    if ((int32_t)(HAL_GetTick() - nextReport) >= 0)
    {
        printf("[%5lu ms] fast=%lu slow=%lu blink=%lu  (fast took %lu ms)\r\n",
               (unsigned long)HAL_GetTick(),
               (unsigned long)fastCount,
               (unsigned long)slowCount,
               (unsigned long)blinkCount,
               (unsigned long)fastThread.runTimeMs());
        nextReport += 1000;
    }
}
