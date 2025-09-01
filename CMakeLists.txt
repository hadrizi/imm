cmake_minimum_required(VERSION 3.20)
project(immersive_sim C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_MAKE_PROGRAM make)

add_subdirectory(external/raylib)

file(GLOB SRC
  src/*.c src/*/*.c src/*/*/*.c
)
add_executable(game ${SRC})
target_include_directories(game PRIVATE src external/raylib/src)
target_link_libraries(game PRIVATE raylib)

# MinGW specifics (explicit system libs)
if (MINGW)
  target_link_libraries(game PRIVATE winmm gdi32 opengl32)
  
  # in case i want to make portable exe
  # target_link_options(game PRIVATE -static -static-libgcc -static-libstdc++)

endif()

# GNU-style warnings when not MSVC
if (NOT MSVC)
  target_compile_options(game PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()
