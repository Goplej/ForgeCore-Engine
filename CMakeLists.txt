cmake_minimum_required(VERSION 3.16)
project(ForgeCoreEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(FC_BUILD_EXAMPLES "Build the tech demo and tools" ON)

# ---------------------------------------------------------------------------
# Renderer backend selection.
#   auto  - use OpenGL if a GL implementation can be found at runtime
#           (probed via dlopen), otherwise fall back to the CPU rasterizer.
#   soft  - always use the built-in CPU rasterizer (headless / CI).
#   gl    - require OpenGL at runtime, fail if unavailable.
# The engine itself is one static library; backends are selected at startup
# so the same binary runs on a desktop (GPU) and headless (CPU) alike.
# ---------------------------------------------------------------------------
set(FC_RENDERER "auto" CACHE STRING "ForgeCore renderer backend: auto | soft | gl")
set_property(CACHE FC_RENDERER PROPERTY STRINGS auto soft gl)

find_library(FC_LIBGL libGL PATHS /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib)
find_library(FC_LIBX11 libX11 PATHS /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib)
message(STATUS "ForgeCore: libGL candidate: ${FC_LIBGL}")
message(STATUS "ForgeCore: libX11 candidate: ${FC_LIBX11}")
message(STATUS "ForgeCore: default renderer: ${FC_RENDERER}")

# ---------------------------------------------------------------------------
# forgecore
# ---------------------------------------------------------------------------
file(GLOB_RECURSE FC_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(forgecore STATIC ${FC_SOURCES})
target_include_directories(forgecore
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(forgecore PRIVATE -O2 -Wall -Wextra)

find_package(Threads REQUIRED)
target_link_libraries(forgecore PUBLIC Threads::Threads)

# dlopen/dlsym (GL probing + runtime backend load)
if(UNIX)
  target_link_libraries(forgecore PUBLIC ${CMAKE_DL_LIBS})
  target_compile_definitions(forgecore PUBLIC FC_PLATFORM_UNIX=1)
endif()

# ---------------------------------------------------------------------------
# examples / tools
# ---------------------------------------------------------------------------
if(FC_BUILD_EXAMPLES)
  add_subdirectory(examples/techdemo)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(test_math tests/test_math.cpp)
target_link_libraries(test_math PRIVATE forgecore)
set_target_properties(test_math PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_test(NAME test_math COMMAND test_math)

add_executable(test_soft tests/test_soft.cpp)
target_link_libraries(test_soft PRIVATE forgecore)
set_target_properties(test_soft PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_test(NAME test_soft COMMAND test_soft)
