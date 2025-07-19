# Copyright 2025 Xenon Emulator Project. All rights reserved.

cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

project(Xenon)

set(COMPILER ${CMAKE_CXX_COMPILER_ID})
message(STATUS "Using ${COMPILER} Version ${CMAKE_CXX_COMPILER_VERSION}")

# Set compiler ID
if (COMPILER STREQUAL "MSVC")
  set(msvc ON)
elseif (COMPILER STREQUAL "GNU")
  set(GCC ON)
elseif (COMPILER STREQUAL "Clang")
  set(Clang ON)
endif()

# Optimizations
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  if (MSVC)
    add_compile_options($<$<CONFIG:Release>:/Oi>)
    add_compile_options($<$<CONFIG:Release>:/Ot>)
  endif()
endif()

# Options
option(GFX_ENABLED "Enable graphics" ON)

# Set Version
set(EMULATOR_VERSION_MAJOR "0")
set(EMULATOR_VERSION_MINOR "0")
set(EMULATOR_VERSION_PATCH "1")

set_source_files_properties(Xenon/Xenon.rc PROPERTIES COMPILE_DEFINITIONS "EMULATOR_VERSION_MAJOR=${EMULATOR_VERSION_MAJOR};EMULATOR_VERSION_MINOR=${EMULATOR_VERSION_MINOR};EMULATOR_VERSION_PATCH=${EMULATOR_VERSION_PATCH}")
add_compile_definitions(APP_VERSION="${EMULATOR_VERSION_MAJOR}.${EMULATOR_VERSION_MINOR}.${EMULATOR_VERSION_PATCH}")

# COMMIT_COUNT on Xenon (with Git)
execute_process(
  COMMAND git rev-list --count HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE COMMIT_COUNT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Xenon BRANCH (with Git)
execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (NOT "${COMMIT_COUNT}" STREQUAL "" AND NOT "${BRANCH}" STREQUAL "")
  add_compile_definitions(COMMIT_COUNT="${COMMIT_COUNT}")
  if (NOT "${BRANCH}" STREQUAL "main")
    add_compile_definitions(BRANCH="${BRANCH}")
    message(STATUS "Building: Xenon Experimental ${EMULATOR_VERSION_MAJOR}.${EMULATOR_VERSION_MINOR}.${EMULATOR_VERSION_PATCH}-${COMMIT_COUNT} - ${BRANCH}")
  else()
    message(STATUS "Building: Xenon Experimental ${EMULATOR_VERSION_MAJOR}.${EMULATOR_VERSION_MINOR}.${EMULATOR_VERSION_PATCH}-${COMMIT_COUNT}")
  endif()
endif()

# Enable SSE3
if (msvc)
  add_compile_options(/arch:SSE2)
elseif (NOT APPLE)
  add_compile_options(-mssse3)
endif()

# Include Deps/ThirdParty and Packages
if (GFX_ENABLED)
  find_package(SDL3 3.2.4 CONFIG)
  find_package(sirit CONFIG)
endif()

find_package(asmjit CONFIG)
find_package(fmt 10.2.1 CONFIG)
find_package(toml11 4.4.0 CONFIG)

include_directories(Xenon)
add_subdirectory(Deps/ThirdParty)

# Include Files
file(GLOB_RECURSE Base Xenon/Base/*.cpp Xenon/Base/*.h)

file(GLOB_RECURSE Core Xenon/Core/*.cpp Xenon/Core/*.h)

if (GFX_ENABLED)
  set(ImGuiDir Deps/ThirdParty/ImGui)
  set(ImGuiBackends
    ${ImGuiDir}/backends/imgui_impl_opengl3.cpp
    ${ImGuiDir}/backends/imgui_impl_opengl3.h
    ${ImGuiDir}/backends/imgui_impl_sdl3.cpp
    ${ImGuiDir}/backends/imgui_impl_sdl3.h
  )
  file(GLOB ImGui ${ImGuiDir}/*.cpp ${ImGuiDir}/*.h)
  file(GLOB_RECURSE XenonRender Xenon/Render/*.cpp Xenon/Render/*.h)
  set(Render
    ${ImGuiBackends}
    ${ImGui}
    ${XenonRender}
  )
endif()

file(GLOB_RECURSE include Xenon/include/*.cpp Xenon/include/*.h)

add_compile_definitions(MICROPROFILE_USE_CONFIG)
set(microprofile_dir Deps/ThirdParty/microprofile)
file(GLOB microprofile ${microprofile_dir}/*.cpp ${microprofile_dir}/*.h)

add_executable(Xenon
  ${Base}
  ${Core}
  ${Render}
  ${include}
  ${microprofile}
  Xenon/Main.cpp
)

target_precompile_headers(Xenon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Xenon/Base/Global.h)

if (GFX_ENABLED)
  add_compile_definitions(GFX_ENABLED)
else()
  add_compile_definitions(NO_GFX)
endif()

if (NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_definitions(DEBUG_BUILD)
endif()

# Link libraries
target_link_libraries(Xenon PRIVATE fmt::fmt toml11::toml11 asmjit::asmjit)
if (GFX_ENABLED)
  target_link_libraries(Xenon PRIVATE glad sirit SDL3::SDL3)
endif()

# Add includes
target_include_directories(Xenon PRIVATE ${microprofile_dir} Xenon/include)
if (GFX_ENABLED)
    target_include_directories(Xenon PRIVATE ${ImGuiDir})
endif()

# Add defines specific to Windows
if (WIN32)
  target_link_libraries(Xenon PRIVATE ws2_32)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

  # Disables Warnings
  add_compile_definitions(_WINSOCK_DEPRECATED_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)

  # Add .rc file, for the app icon and executable properties
  target_sources(Xenon PRIVATE Xenon/Xenon.rc)
endif()

# Specific to MSVC, forces system version so the autodetected 8.1 SDK isn't used
# Also enables multi-processor (multithreaded) compiling on MSVC
if (CMAKE_GENERATOR MATCHES "Visual Studio")
  set(CMAKE_GENERATOR_PLATFORM x64)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()
