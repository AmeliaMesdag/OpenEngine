cmake_minimum_required(VERSION 3.20)

project(Memory LANGUAGES CXX)

# Source/MimallocOverrides.cpp is not part of the module: Apps/Editor and GameEngine.Native
# compile it themselves when ENABLE_MIMALLOC is ON.
set(MEMORY_SOURCES
    Source/AllocationCounter.cpp
    Source/Arena.cpp
    Source/StatsRegistry.cpp
    Source/TrackedBytes.cpp
)

add_library(Memory OBJECT ${MEMORY_SOURCES})

target_compile_features(Memory PUBLIC cxx_std_20)

target_include_directories(Memory
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
    PRIVATE
        Source
        Testing
)

# A target that links Memory (or Engine, which re-exports this definition) reaches the
# process's one allocation counter here, so the copy of AllocationCounter.cpp that
# cmake/AllocationHook.cmake adds to test executables compiles empty in it.
target_compile_definitions(Memory INTERFACE GE_ALLOCATION_COUNTER_LINKED)

# Types only, so every other module, the logger and the platform layer included, can
# allocate through Memory without a dependency cycle.
target_link_libraries(Memory PUBLIC Types)

if(MSVC)
    target_compile_options(Memory PRIVATE /W4)
else()
    target_compile_options(Memory PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_subdirectory(Tests)
endif()

if(NOT EMSCRIPTEN)
    add_subdirectory(Bench)
endif()
