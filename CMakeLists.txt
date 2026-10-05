cmake_minimum_required(VERSION 3.20)

project(MiniECS
    VERSION 1.2.0
    DESCRIPTION "A lightweight Entity Component System implemented from scratch in modern C++"
    LANGUAGES CXX
)

option(MINIECS_BUILD_EXAMPLES   "Build the example program" ON)
option(MINIECS_BUILD_TESTS      "Build the unit tests"      ON)
option(MINIECS_BUILD_BENCHMARKS "Build the benchmarks"      ON)
option(MINIECS_BUILD_DEMO       "Build the raylib boids demo (downloads raylib)" ON)
option(MINIECS_SANITIZERS       "Enable ASan/UBSan (GCC/Clang only)" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Warnings -----------------------------------------------------------------

add_library(miniecs_warnings INTERFACE)

if(MSVC)
    target_compile_options(miniecs_warnings INTERFACE /W4 /permissive-)
else()
    target_compile_options(miniecs_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

if(MINIECS_SANITIZERS AND NOT MSVC)
    target_compile_options(miniecs_warnings INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(miniecs_warnings INTERFACE -fsanitize=address,undefined)
endif()

# ---- Library ------------------------------------------------------------------

add_library(MiniECS STATIC
    src/EntityManager.cpp
)

target_include_directories(MiniECS
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(MiniECS PRIVATE miniecs_warnings)

# ---- Example ------------------------------------------------------------------

if(MINIECS_BUILD_EXAMPLES)
    add_executable(MiniECS_example examples/main.cpp)
    target_link_libraries(MiniECS_example PRIVATE MiniECS miniecs_warnings)
endif()

# ---- Tests --------------------------------------------------------------------

if(MINIECS_BUILD_TESTS)
    enable_testing()

    add_executable(MiniECS_tests tests/ecs_tests.cpp)
    target_link_libraries(MiniECS_tests PRIVATE MiniECS miniecs_warnings)

    add_test(NAME MiniECS_tests COMMAND MiniECS_tests)
endif()

# ---- Benchmarks ---------------------------------------------------------------

if(MINIECS_BUILD_BENCHMARKS)
    add_executable(MiniECS_benchmark benchmarks/ecs_benchmark.cpp)
    target_link_libraries(MiniECS_benchmark PRIVATE MiniECS miniecs_warnings)

    add_executable(MiniECS_memory_benchmark benchmarks/memory_benchmark.cpp)
    target_link_libraries(MiniECS_memory_benchmark PRIVATE MiniECS miniecs_warnings)
endif()

# ---- Boids demo (raylib) ------------------------------------------------------

if(MINIECS_BUILD_DEMO)
    find_package(raylib 5.5 QUIET)

    if(NOT raylib_FOUND)
        include(FetchContent)

        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG        5.5
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(raylib)

        # Don't apply our warnings to raylib's headers.
        get_target_property(RAYLIB_INCLUDES raylib INTERFACE_INCLUDE_DIRECTORIES)
        set_target_properties(raylib PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDES}")
    endif()

    add_executable(MiniECS_boids
        demo/main.cpp
        demo/Components.h
        demo/SpatialGrid.h
        demo/Simulation.h
        demo/Render.h
    )
    target_link_libraries(MiniECS_boids PRIVATE MiniECS raylib miniecs_warnings)

    if(MSVC)
        # No console window in Release.
        set_target_properties(MiniECS_boids PROPERTIES
            LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
    endif()

    set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT MiniECS_boids)
endif()
