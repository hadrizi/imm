#ifndef CORE_H
#define CORE_H

#if defined(_WIN32) && defined(BUILD_LIBTYPE_SHARED)
    #define CORE __declspec(dllexport) // building core as a Win32 shared library (.dll)
#elif defined(_WIN32) && defined(USE_CORE_LIB_SHARED)
    #define CORE __declspec(dllimport) // using core as a Win32 shared library (.dll)
#else
    #define CORE // building or using core as a static library
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

typedef struct {
    Config config;
    Camera camera;
} GameState;

typedef void (*core_create_window_fn)();
typedef i32  (*core_prepare_fn)();
typedef void (*core_loop_fn)();
typedef bool (*core_window_should_close_fn)();
typedef void (*core_close_window_fn)();
typedef GameState (*core_get_state_fn)();
typedef void (*core_set_state_fn)(GameState);

#endif /* CORE_H */
