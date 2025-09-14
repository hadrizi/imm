#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "raylib.h"
#include "core.h"

typedef struct BoxDef { Vector3 pos, size; Color color; } BoxDef;

static const BoxDef levelBoxes[] = {
    {{ 0, 1.0f,  0}, { 1, 1, 1},       RED},       // center block (low)
    {{ 4, 1.0f,  0}, { 2, 2, 2},        ORANGE},    // right
    {{-4, 1.0f,  0}, { 2, 2, 2},        BLUE},      // left
    {{ 0, 2.0f, -6}, {10, 0.5f, 2},     DARKGRAY},  // “wall” ahead
    {{ 0, 0.0f,  0}, {50, 0.1f, 50},    LIGHTGRAY}  // a thin “ground”
};
static const int levelBoxesCount = (int)(sizeof(levelBoxes)/sizeof(levelBoxes[0]));

static Config cfg = {0};
static Camera camera = {0};

// toml parser; ignores sections
static bool load_config_toml(const char* path, Config* out) {
    char* text = LoadFileText(path);
    if (!text) return false;

    for (char* line = text; *line;) {
        char* eol = line; while (*eol && *eol != '\n' && *eol != '\r') ++eol;
        *eol = '\0';
        
        // parse keys
        if (strstr(line, "width"))      out->width      =        atoi(strchr(line, '=')+1);
        if (strstr(line, "height"))     out->height     =        atoi(strchr(line, '=')+1);
        if (strstr(line, "fov"))        out->fov        = (float)atof(strchr(line, '=')+1);
        if (strstr(line, "target_fps")) out->target_fps =        atoi(strchr(line, '=')+1);

        line = eol + 1;
    }
    UnloadFileText(text);
    return true;
}

void core_create_window() {
    SetTargetFPS(cfg.target_fps);
    InitWindow(cfg.width, cfg.height, "imm prototype");
    DisableCursor();
}

i32 core_prepare() {
    lstring cfgPath = "assets/config.toml";
    if (load_config_toml(cfgPath, &cfg) == false){
        swiss_log_error("failed to load config");
        return 1;
    }
    
    cfg.path = cfgPath;
    cfg._next_check = 0.0;
    cfg._last_mod = GetFileModTime(cfgPath);

    camera.position   = (Vector3){ 0.0f, 2.0f, 10.0f };     // Camera position
    camera.target     = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
    camera.up         = (Vector3){ 0.0f, 1.0f, 0.0f };      // Camera up vector (rotation towards target)
    camera.fovy       = 45.0f;                              // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;                 // Camera mode type

    return 0;
}

void core_loop() {
    UpdateCamera(&camera, CAMERA_FIRST_PERSON);

    double now = GetTime();
    if (now >= cfg._next_check) {
        cfg._next_check = now + 0.5;
        long mod = GetFileModTime(cfg.path);
        if (mod != cfg._last_mod) {
            cfg._last_mod = mod;
            if (load_config_toml(cfg.path, &cfg)) {
                camera.fovy = cfg.fov;
                SetTargetFPS(cfg.target_fps);
                SetWindowSize(cfg.width, cfg.height);
            }
        }
    }

    if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

    BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);
            for (int i = 0; i < levelBoxesCount; ++i) {
                DrawCubeV(levelBoxes[i].pos, levelBoxes[i].size, levelBoxes[i].color);
                DrawCubeWiresV(levelBoxes[i].pos, levelBoxes[i].size, levelBoxes[i].color);
            }
            DrawGrid(100, 1.0f);
        EndMode3D();
        DrawFPS(10, 10);
        DrawText(TextFormat("FOV: %.1f | FPS cap: %d | F1: toggle lock | F11: fullscreen", 
                cfg.fov, cfg.target_fps), 10, 40, 20, DARKGRAY);
    EndDrawing();
}

bool core_window_should_close() {
    return WindowShouldClose();
}

void core_close_window() {
    CloseWindow();
}
