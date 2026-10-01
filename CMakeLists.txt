cmake_minimum_required(VERSION 3.20)

project(MiniECS
    VERSION 1.0.0
    DESCRIPTION "A lightweight Entity Component System implemented from scratch in modern C++"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(MiniECS
    examples/main.cpp
    src/EntityManager.cpp
)

target_include_directories(MiniECS
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)