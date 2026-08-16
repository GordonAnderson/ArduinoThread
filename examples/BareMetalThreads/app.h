/*
    app.h - C entry points for the C++ application layer.

    Thread and ThreadController are C++ classes, but CubeMX generates main.c
    and rewrites it on every code generation. Rather than converting main.c to
    C++ (which CubeMX would undo), the application lives in app.cpp and is
    reached through these two C-linkage functions.

    Insert the calls inside CubeMX's USER CODE markers so they survive
    regeneration:

        // USER CODE BEGIN Includes
        #include "app.h"
        // USER CODE END Includes

        // USER CODE BEGIN 2
        app_setup();
        // USER CODE END 2

        while (1)
        {
            // USER CODE BEGIN 3
            app_loop();
        }
        // USER CODE END 3

    GAA Custom Electronics, LLC
*/

#ifndef app_h
#define app_h

#ifdef __cplusplus
extern "C" {
#endif

void app_setup(void);   /* call once, after all MX_*_Init() */
void app_loop(void);     /* call every pass of the main while(1) */

#ifdef __cplusplus
}
#endif

#endif /* app_h */
