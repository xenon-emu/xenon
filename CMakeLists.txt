# Copyright 2025 Xenon Emulator Project

cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Set compiler ID
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(msvc TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(gcc TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(clang TRUE)
endif()

# Optimizations
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
if (msvc)
  add_compile_options($<$<CONFIG:Release>:/Oi>)
  add_compile_options($<$<CONFIG:Release>:/Ot>)
elseif(clang AND WIN32)
  add_compile_options($<$<CONFIG:Release>:-Ofast>)
else()
  add_compile_options($<$<CONFIG:Release>:-O3>)
endif()

project(Xenon)

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
    message(STATUS "Title: Xenon Experimental ${EMULATOR_VERSION_MAJOR}.${EMULATOR_VERSION_MINOR}.${EMULATOR_VERSION_PATCH}-${COMMIT_COUNT} - ${BRANCH}")
  else()
    message(STATUS "Title: Xenon Experimental ${EMULATOR_VERSION_MAJOR}.${EMULATOR_VERSION_MINOR}.${EMULATOR_VERSION_PATCH}-${COMMIT_COUNT}")
  endif()
endif()

# Make microprofile use a custom config
add_compile_definitions(MICROPROFILE_USE_CONFIG)


# Include Deps/ThirdParty and Packages
if (GFX_ENABLED)
  find_package(SDL3 3.2.4 CONFIG)
  find_package(sirit)
endif()

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

target_precompile_headers(Xenon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Xenon/Base/Types.h)

if (GFX_ENABLED)
  add_compile_definitions(GFX_ENABLED)
else()
  add_compile_definitions(NO_GFX)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR "Debug")
  add_compile_definitions(DEBUG_BUILD)
endif()

# Copy fonts to build directory
if (GFX_ENABLED)
  add_custom_target(copy-fonts
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/Deps/fonts" # Input
    "${CMAKE_CURRENT_BINARY_DIR}/fonts"  # Output
    COMMENT "Copying fonts (${CMAKE_CURRENT_SOURCE_DIR}/Deps/fonts) to build directory (${CMAKE_CURRENT_BINARY_DIR}/fonts)"
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Deps/fonts)
endif()


# Link libraries
target_link_libraries(Xenon PRIVATE fmt::fmt sirit toml11::toml11)
if (GFX_ENABLED)
  target_link_libraries(Xenon PRIVATE glad SDL3::SDL3)
endif()

# Add includes
target_include_directories(Xenon PRIVATE ${microprofile_dir} Xenon/include)
if (GFX_ENABLED)
    target_include_directories(Xenon PRIVATE ${ImGuiDir})
endif()

# Add fonts target
if (GFX_ENABLED)
  add_dependencies(Xenon copy-fonts)
endif()


# Add defines specific to windows
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
if (msvc AND NOT clang)
  set(CMAKE_GENERATOR_PLATFORM x64)
  set(CMAKE_SYSTEM_VERSION 10.0.19041.0)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()
