cmake_minimum_required(VERSION 3.20)
project(immersive_sim C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_MAKE_PROGRAM make)

# SOURCES =================================
file(GLOB SRC
  src/*.c src/*/*.c src/*/*/*.c
)

set(CORE_SRC src/core.c)
set(GAME_SRC ${SRC})
list(REMOVE_ITEM GAME_SRC ${CORE_SRC})
# =========================================

# DEPENDENCIES ============================
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
add_subdirectory(external/raylib)

add_library(swiss_lib INTERFACE)
target_include_directories(swiss_lib INTERFACE external/swiss)
# =========================================

# CORE SHARED LIB =========================
add_library(core MODULE ${CORE_SRC} src/core.h)
set_target_properties(core PROPERTIES OUTPUT_NAME core_build)
target_compile_definitions(core PRIVATE BUILD_LIBTYPE_SHARED SWISS_IMPL)
target_include_directories(core PRIVATE src external/raylib/src external/swiss)
target_link_libraries(core PUBLIC raylib swiss_lib)

if (WIN32)
  set_target_properties(core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
# =========================================

# GAME ====================================
add_executable(game ${GAME_SRC})
target_compile_definitions(game PRIVATE USE_CORE_LIB_SHARED)
target_include_directories(game PRIVATE src external/raylib/src external/swiss)
target_link_libraries(game PUBLIC raylib swiss_lib)
# =========================================

# MISC ====================================
# MinGW specifics (explicit system libs)
if (MINGW)
  target_link_libraries(game PRIVATE winmm gdi32 opengl32 shlwapi)
  
  # in case i want to make portable exe
  # target_link_options(game PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# GNU-style warnings when not MSVC
if (NOT MSVC)
  target_compile_options(game PRIVATE -Wall -Wextra -Wconversion -Wsign-conversion)
endif()

if (WIN32)
  # Swap libcore.dll atomically using the built plugin (libcore_build.dll / core_build.dll)
  add_custom_command(TARGET core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:game>
    COMMAND ${CMAKE_COMMAND} -E copy
      $<TARGET_FILE:core>
      $<TARGET_FILE_DIR:game>/libcore.dll.tmp
    COMMAND ${CMAKE_COMMAND} -E rename
      $<TARGET_FILE_DIR:game>/libcore.dll.tmp
      $<TARGET_FILE_DIR:game>/libcore.dll
    # optional: remove the build artifact if you don’t want it lying around
    COMMAND ${CMAKE_COMMAND} -E remove -f $<TARGET_FILE:core>
  )
  # Ensure the single shared raylib is next to the EXE (both game and core use this one)
  add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:raylib>
      $<TARGET_FILE_DIR:game>
  )
endif()
# =========================================
