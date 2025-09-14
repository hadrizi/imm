#ifndef CORE_H
#define CORE_H

#if defined(_WIN32) && defined(BUILD_LIBTYPE_SHARED)
    #define CORE __declspec(dllexport) // We are building core as a Win32 shared library (.dll)
#elif defined(_WIN32) && defined(USE_CORE_LIB_SHARED)
    #define CORE __declspec(dllimport) // We are using core as a Win32 shared library (.dll)
#else
    #define CORE // We are building or using core as a static library
#endif

#include "raylib.h"
#include "swiss.h"

typedef struct {
    i32     width;
    i32     height;
    f32     fov;
    i32     target_fps;
    lstring path;

    // operational data
    f64 _next_check;
    i64 _last_mod;
} Config;

void core_create_window();
i32  core_prepare();
void core_loop();
bool core_window_should_close();
void core_close_window();

#endif /* CORE_H */
