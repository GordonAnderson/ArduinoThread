/*
    gthread_compat.h - dual-build compatibility layer for ArduinoThread

    Lets Thread/ThreadController compile unchanged on both the Arduino
    framework and bare STM32Cube (HAL), from identical sources.

    The library needs exactly one thing from its host: a free-running
    millisecond tick.

      * Arduino builds  -> Arduino's millis()
      * Pure-Cube builds -> HAL_GetTick()

    Deliberately standalone: this does NOT include gaace_compat.h. Thread is
    usable without GAACE_Core and that independence is worth keeping. The
    ten-line duplication is cheaper than the coupling.

    millis() is declared static inline here, so a translation unit that
    includes both this and GAACE_Core's compat layer has no ODR conflict —
    each gets internal linkage.

    Overriding the tick source
    --------------------------
    GTHREAD_MILLIS may be set to any expression returning uint32_t
    milliseconds - for a project using an RTOS tick or a hardware timer
    instead of SysTick, or for host-side unit tests.

    It MUST be set as a GLOBAL BUILD FLAG, not a #define in one source file.
    Thread.cpp and ThreadController.cpp are separate translation units and
    will not see a #define made in main.cpp - they would silently fall back to
    HAL_GetTick() while the application used the override, giving two
    different clocks in one program.

        platformio.ini:
            build_flags =
                -DGTHREAD_MILLIS=my_tick_source()
                -include my_tick.h        ; must declare my_tick_source()

    The declaration has to be visible in every translation unit too, hence
    the -include. If that is awkward, prefer GTHREAD_NO_HAL below and simply
    provide your own HAL_GetTick().

    GAA Custom Electronics, LLC
*/

#ifndef gthread_compat_h
#define gthread_compat_h

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(ARDUINO)

    /* ----------------------------------------------------------------- */
    /* Arduino path - millis(), String, etc. all come from the framework  */
    /* ----------------------------------------------------------------- */
    #include <Arduino.h>

#else

    /* ----------------------------------------------------------------- */
    /* Pure STM32Cube path                                               */
    /* ----------------------------------------------------------------- */

    #if !defined(GTHREAD_MILLIS)

        /* Resolve the tick to HAL_GetTick().

           By default the series HAL header is pulled in, selected from the
           project's build flags (STM32H743xx, STM32F407xx, ...).

           Define GTHREAD_NO_HAL to skip that include and forward-declare
           HAL_GetTick() instead. Useful for host-side unit tests, and on real
           targets where dragging the full HAL header into every translation
           unit is unwelcome — the symbol still resolves at link time. */
        #if defined(GTHREAD_NO_HAL)
            #ifdef __cplusplus
            extern "C" {
            #endif
            uint32_t HAL_GetTick(void);
            #ifdef __cplusplus
            }
            #endif
        #else
            #if   defined(STM32H7)  || defined(STM32H743xx) || defined(STM32H753xx) \
               || defined(STM32H750xx)
                #include "stm32h7xx_hal.h"
            #elif defined(STM32F4)  || defined(STM32F407xx) || defined(STM32F429xx)
                #include "stm32f4xx_hal.h"
            #elif defined(STM32G4)
                #include "stm32g4xx_hal.h"
            #elif defined(STM32L4)
                #include "stm32l4xx_hal.h"
            #else
                #error "gthread_compat.h: unknown STM32 series. Define GTHREAD_MILLIS to your own tick source, use GTHREAD_NO_HAL, or add the series header here."
            #endif
        #endif

        #define GTHREAD_MILLIS  HAL_GetTick()

    #endif /* !GTHREAD_MILLIS */

    /* Thread's own sources call millis(); map it to the configured tick.
       static inline -> internal linkage, no ODR clash with other libraries
       that provide their own millis() shim. */
    static inline uint32_t millis(void)
    {
        return (uint32_t)(GTHREAD_MILLIS);
    }

#endif /* ARDUINO */

#endif /* gthread_compat_h */
