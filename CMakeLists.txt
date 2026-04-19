#***************************************************************
#* Copyright 2026 Xenon Emulator Project. All rights reserved. *
#***************************************************************

cmake_minimum_required(VERSION 3.22)

project(Xenon)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(COMPILER ${CMAKE_CXX_COMPILER_ID})
message(STATUS "Using ${COMPILER} Version ${CMAKE_CXX_COMPILER_VERSION}")

if (COMPILER STREQUAL "MSVC")
  set(msvc ON)
elseif (COMPILER STREQUAL "GNU")
  set(GCC ON)
elseif (COMPILER STREQUAL "Clang")
  set(Clang ON)
endif()

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  if (msvc)
    add_compile_options($<$<CONFIG:Release>:/Oi> $<$<CONFIG:Release>:/Ot>)
  endif()
endif()

# Silences clang-cl: warning: argument unused during compilation: '/Zc:preprocessor' [-Wunused-command-line-argument]
if (MSVC AND Clang)
  add_compile_options(-Wno-unused-command-line-argument)
endif()

#
# Options
#
option(GFX_ENABLED "Enable graphics" ON)
option(XENON_USE_SYSTEM_DEPS "Prefer system-installed packages (find_package first)" ON)
option(XENON_ALLOW_BUNDLED_DEPS "If a package isn't found, fall back to bundled subdirs" ON)
option(XE_ASMJIT_NEW_OUT_API "New AsmJit API" ON)
set(XENON_THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Deps/ThirdParty" CACHE PATH "Bundled deps root")

#
# Version
#
set(EMULATOR_VERSION "0.0.1")

string(REPLACE "." ";" EMULATOR_VERSION_LIST "${EMULATOR_VERSION}")
list(LENGTH EMULATOR_VERSION_LIST EMULATOR_VERSION_LIST_LEN)
if (NOT EMULATOR_VERSION_LIST_LEN EQUAL 3)
  message(FATAL_ERROR "EMULATOR_VERSION must be in major.minor.patch format")
endif()

list(GET EMULATOR_VERSION_LIST 0 EMULATOR_VERSION_MAJOR)
list(GET EMULATOR_VERSION_LIST 1 EMULATOR_VERSION_MINOR)
list(GET EMULATOR_VERSION_LIST 2 EMULATOR_VERSION_PATCH)

set_property(SOURCE Xenon/Xenon.rc APPEND PROPERTY COMPILE_DEFINITIONS
  EMULATOR_VERSION_MAJOR=${EMULATOR_VERSION_MAJOR}
  EMULATOR_VERSION_MINOR=${EMULATOR_VERSION_MINOR}
  EMULATOR_VERSION_PATCH=${EMULATOR_VERSION_PATCH}
)

add_compile_definitions(APP_VERSION="${EMULATOR_VERSION}")

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

# CPU features
if (msvc)
  add_compile_options(/arch:SSE2)
elseif (NOT APPLE)
  add_compile_options(-mssse3)
endif()

#
# Dependency Resoultion
#

function(xenon_use_dep pkg subdir)
  set(options)
  set(oneValueArgs SYSTEM_TARGET BUNDLED_TARGET OUT_VAR)
  cmake_parse_arguments(XUD "${options}" "${oneValueArgs}" "" ${ARGN})

  if (XENON_USE_SYSTEM_DEPS)
    find_package(${pkg} CONFIG QUIET)
    if (XUD_SYSTEM_TARGET AND TARGET ${XUD_SYSTEM_TARGET})
      set(_resolved_target ${XUD_SYSTEM_TARGET})
    endif()
  endif()

  if (NOT _resolved_target AND XENON_ALLOW_BUNDLED_DEPS)
    message(STATUS "Using bundled ${pkg}")
    add_subdirectory(${XENON_THIRDPARTY_DIR}/${subdir} ${subdir} EXCLUDE_FROM_ALL)

    if (XUD_BUNDLED_TARGET AND TARGET ${XUD_BUNDLED_TARGET})
      set(_resolved_target ${XUD_BUNDLED_TARGET})
    elseif (XUD_SYSTEM_TARGET AND TARGET ${XUD_SYSTEM_TARGET})
      set(_resolved_target ${XUD_SYSTEM_TARGET})
    endif()
  endif()

  if (NOT _resolved_target)
    message(FATAL_ERROR "Failed to resolve dependency ${pkg}")
  endif()

  if (XUD_OUT_VAR)
    set(${XUD_OUT_VAR} ${_resolved_target} PARENT_SCOPE)
  endif()

  message(STATUS "Resolved ${pkg} -> ${_resolved_target}")
endfunction()

xenon_use_dep(fmt fmt
  SYSTEM_TARGET fmt::fmt
  BUNDLED_TARGET fmt::fmt
  OUT_VAR XENON_FMT_TARGET
)

xenon_use_dep(asmjit asmjit
  SYSTEM_TARGET asmjit::asmjit
  BUNDLED_TARGET asmjit
  OUT_VAR XENON_ASMJIT_TARGET
)

xenon_use_dep(toml11 toml11
  SYSTEM_TARGET toml11::toml11
  BUNDLED_TARGET toml11::toml11
  OUT_VAR XENON_TOML11_TARGET
)

if (GFX_ENABLED)
  xenon_use_dep(SDL3 SDL3
    SYSTEM_TARGET SDL3::SDL3
    BUNDLED_TARGET SDL3::SDL3
    OUT_VAR XENON_SDL3_TARGET
  )

  xenon_use_dep(VulkanHeaders VulkanHeaders
    SYSTEM_TARGET Vulkan::Headers
    BUNDLED_TARGET Vulkan::Headers
    OUT_VAR XENON_VULKAN_HEADERS_TARGET
  )

  xenon_use_dep(VulkanMemoryAllocator VulkanMemoryAllocator
    SYSTEM_TARGET GPUOpen::VulkanMemoryAllocator
    BUNDLED_TARGET VulkanMemoryAllocator
    OUT_VAR XENON_VMA_TARGET
  )

  xenon_use_dep(vk-bootstrap vk-bootstrap
    SYSTEM_TARGET vk-bootstrap::vk-bootstrap
    BUNDLED_TARGET vk-bootstrap
    OUT_VAR XENON_VK_BOOTSTRAP_TARGET
  )

  xenon_use_dep(glslang glslang
    SYSTEM_TARGET glslang::glslang
    BUNDLED_TARGET glslang
    OUT_VAR XENON_GLSLANG_TARGET
  )
endif()

# These will always be static.
# ImGui is not something you can package easily, and Sirit is a fork.
# glad is per-settings
if (GFX_ENABLED)
  set(ImGuiDir ${XENON_THIRDPARTY_DIR}/ImGui)
  set(SiritDir ${XENON_THIRDPARTY_DIR}/Sirit)
  set(gladDir ${XENON_THIRDPARTY_DIR}/glad)
endif()

set(microprofile_dir ${XENON_THIRDPARTY_DIR}/microprofile)
set(plusaes_dir ${XENON_THIRDPARTY_DIR}/plusaes)

#
# Sources
#

include_directories(Xenon)

if (XE_ASMJIT_NEW_OUT_API)
  add_compile_definitions(XE_ASMJIT_NEW_OUT_API)
endif()

file(GLOB_RECURSE Base Xenon/Base/*.cpp Xenon/Base/*.h)
file(GLOB_RECURSE Core Xenon/Core/*.cpp Xenon/Core/*.h)
file(GLOB_RECURSE include Xenon/include/*.cpp Xenon/include/*.h)

if (GFX_ENABLED)
  add_compile_definitions(VK_NO_PROTOTYPES)

  set(ImGuiBackends
    ${ImGuiDir}/backends/imgui_impl_opengl3.cpp
    ${ImGuiDir}/backends/imgui_impl_opengl3.h
    ${ImGuiDir}/backends/imgui_impl_vulkan.cpp
    ${ImGuiDir}/backends/imgui_impl_vulkan.h
    ${ImGuiDir}/backends/imgui_impl_sdl3.cpp
    ${ImGuiDir}/backends/imgui_impl_sdl3.h
  )

  file(GLOB ImGui ${ImGuiDir}/*.cpp ${ImGuiDir}/*.h)
  file(GLOB_RECURSE XenonRender Xenon/Render/*.cpp Xenon/Render/*.h)

  add_subdirectory(${SiritDir} Sirit EXCLUDE_FROM_ALL)
  add_subdirectory(${gladDir} glad EXCLUDE_FROM_ALL)

  if (TARGET sirit)
    set(XENON_SIRIT_TARGET sirit)
  elseif (TARGET Sirit)
    set(XENON_SIRIT_TARGET Sirit)
  elseif (TARGET sirit::sirit)
    set(XENON_SIRIT_TARGET sirit::sirit)
  else()
    get_cmake_property(_targets TARGETS)
    message(FATAL_ERROR "Bundled Sirit target not found. Targets: ${_targets}")
  endif()

  set(Render
    ${ImGuiBackends}
    ${ImGui}
    ${XenonRender}
  )
endif()

add_compile_definitions(MICROPROFILE_USE_CONFIG)
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

#
# Linking
#

target_link_libraries(Xenon PRIVATE
  ${XENON_FMT_TARGET}
  ${XENON_TOML11_TARGET}
  ${XENON_ASMJIT_TARGET}
)

if (GFX_ENABLED)
  target_link_libraries(Xenon PRIVATE
    glad
    ${XENON_GLSLANG_TARGET}
    ${XENON_SIRIT_TARGET}
    ${XENON_SDL3_TARGET}
    ${XENON_VULKAN_HEADERS_TARGET}
    ${XENON_VMA_TARGET}
    ${XENON_VK_BOOTSTRAP_TARGET}
  )
endif()

if (WIN32)
  # cryptopp on Windows often needs explicit link, better to be safe than sorry
  find_package(cryptopp CONFIG QUIET)
  if (TARGET cryptopp::cryptopp)
    target_link_libraries(Xenon PRIVATE cryptopp::cryptopp)
  elseif (TARGET cryptopp)
    target_link_libraries(Xenon PRIVATE cryptopp)
  endif()
endif()

#
# Includes
#

target_include_directories(Xenon PRIVATE
  ${microprofile_dir}
  ${plusaes_dir}/include
  Xenon/include
  Xenon
)
if (GFX_ENABLED)
  target_include_directories(Xenon SYSTEM PRIVATE
    ${ImGuiDir}
    ${SiritDir}/include
    ${gladDir}/include
  )
endif()

#
# Platform
#

if (APPLE)
  add_compile_definitions(SIMPLE_FMT)
elseif (WIN32)
  # Synchronization is needed for WaitOnAddress/WakeByAddressSingle
  target_link_libraries(Xenon PRIVATE iphlpapi setupapi ws2_32 synchronization)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

  # Disables Warnings
  add_compile_definitions(_WINSOCK_DEPRECATED_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)

  # Add .rc file, for the app icon and executable properties
  target_sources(Xenon PRIVATE Xenon/Xenon.rc)
endif()

# This forces system version so the autodetected 8.1 SDK isn't used
# Also enables multi-processor (multi-threaded) compiling
if (CMAKE_GENERATOR MATCHES "Visual Studio")
  set(CMAKE_GENERATOR_PLATFORM x64)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()
