# Copyright 2025 Xenon Emulator Project. All rights reserved.

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
  if (msvc)
    add_compile_options($<$<CONFIG:Release>:/Oi> $<$<CONFIG:Release>:/Ot>)
  endif()
endif()

# Silences clang-cl: warning: argument unused during compilation: '/Zc:preprocessor' [-Wunused-command-line-argument]
if (MSVC AND Clang)
  add_compile_options(-Wno-unused-command-line-argument)
endif()

# Options
option(GFX_ENABLED "Enable graphics" ON)
option(XENON_USE_SYSTEM_DEPS "Prefer system-installed packages (find_package first)" ON)
option(XENON_ALLOW_BUNDLED_DEPS "If a package isn't found, fall back to bundled subdirs" ON)
set(XENON_THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Deps/ThirdParty" CACHE PATH "Bundled deps root")

# Version
set(EMULATOR_VERSION_MAJOR "0")
set(EMULATOR_VERSION_MINOR "0")
set(EMULATOR_VERSION_PATCH "1")

set_property(SOURCE Xenon/Xenon.rc APPEND PROPERTY COMPILE_DEFINITIONS
  EMULATOR_VERSION_MAJOR=${EMULATOR_VERSION_MAJOR}
  EMULATOR_VERSION_MINOR=${EMULATOR_VERSION_MINOR}
  EMULATOR_VERSION_PATCH=${EMULATOR_VERSION_PATCH}
)
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

# CPU features
if (msvc)
  add_compile_options(/arch:SSE2)
elseif (NOT APPLE)
  add_compile_options(-mssse3)
endif()

# Try system first, else add_subdirectory for the specific dep
macro(xenon_dep pkg target subdir)
  set(found_system OFF)
  if (XENON_USE_SYSTEM_DEPS)
    # Try config mode first, then module mode
    find_package(${pkg} CONFIG QUIET)
    if (NOT TARGET ${target})
      find_package(${pkg} QUIET)
    endif()

    if (TARGET ${target})
      # Check for library or header-only target
      get_target_property(lib_location ${target} IMPORTED_LOCATION)
      get_target_property(lib_location_debug ${target} IMPORTED_LOCATION_DEBUG)
      get_target_property(lib_location_release ${target} IMPORTED_LOCATION_RELEASE)
      get_target_property(type ${target} TYPE)

      if (lib_location OR lib_location_debug OR lib_location_release)
        message(STATUS "Using system ${pkg} (prebuilt library)")
        set(found_system ON)
      elseif (type STREQUAL "STATIC_LIBRARY" OR type STREQUAL "MODULE_LIBRARY")
        message(STATUS "Using system ${pkg} (build target)")
        set(found_system ON)
      elseif (type STREQUAL "INTERFACE_LIBRARY")
        message(STATUS "Using system ${pkg} (header-only)")
        set(found_system ON)
      else()
        message(WARNING "System ${pkg} found but unusable - falling back to bundled")
      endif()
    endif()
  endif()

  if (NOT found_system)
    if (NOT XENON_ALLOW_BUNDLED_DEPS)
      message(FATAL_ERROR "Package ${pkg} not found and bundled deps disabled (XENON_ALLOW_BUNDLED_DEPS=OFF).")
    endif()

    if (NOT TARGET ${target})
      # Per-dep settings
      if (${pkg} STREQUAL "toml11")
        set(TOML11_INSTALL OFF CACHE BOOL "" FORCE)
      elseif (${pkg} STREQUAL "SDL3")
        set(SDL_DISKAUDIO OFF CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
      elseif (${pkg} STREQUAL "asmjit")
        set(ASMJIT_NO_CUSTOM_FLAGS TRUE CACHE BOOL "" FORCE)
        set(ASMJIT_STATIC TRUE CACHE BOOL "" FORCE)
      elseif (${pkg} STREQUAL "Vulkan-Headers")
        # Vulkan-Headers is header-only, need to expose the include directory
        set(VULKAN_HEADERS_INSTALL OFF CACHE BOOL "" FORCE)
        add_subdirectory(${subdir})
        # Ensure include dir is propagated
        set_target_properties(Vulkan::Headers PROPERTIES
          INTERFACE_INCLUDE_DIRECTORIES
          ${CMAKE_CURRENT_SOURCE_DIR}/Deps/ThirdParty/Vulkan-Headers/include
        )
        set(found_system ON)
      endif()

      message(STATUS "Using bundled ${pkg}")
      add_subdirectory(${XENON_THIRDPARTY_DIR}/${subdir} ${subdir} EXCLUDE_FROM_ALL)
    endif()
  endif()
endmacro()

xenon_dep(fmt fmt::fmt fmt)
xenon_dep(asmjit asmjit::asmjit asmjit)
xenon_dep(toml11 toml11::toml11 toml11)
if (GFX_ENABLED)
  xenon_dep(glad glad glad)
  xenon_dep(SDL3 SDL3::SDL3 SDL3)
  xenon_dep(sirit sirit Sirit)
  xenon_dep(VulkanHeaders Vulkan::Headers Vulkan-Headers)
  xenon_dep(VulkanMemoryAllocator VulkanMemoryAllocator VulkanMemoryAllocator)
endif()

# Include Files
include_directories(Xenon)

file(GLOB_RECURSE Base Xenon/Base/*.cpp Xenon/Base/*.h)
file(GLOB_RECURSE Core Xenon/Core/*.cpp Xenon/Core/*.h)

if (GFX_ENABLED)
  add_compile_definitions(VK_NO_PROTOTYPES)
  set(ImGuiDir ${XENON_THIRDPARTY_DIR}/ImGui)
  set(VolkDir ${XENON_THIRDPARTY_DIR}/volk)
  set(ImGuiBackends
    ${ImGuiDir}/backends/imgui_impl_opengl3.cpp
    ${ImGuiDir}/backends/imgui_impl_opengl3.h
    ${ImGuiDir}/backends/imgui_impl_vulkan.cpp
    ${ImGuiDir}/backends/imgui_impl_vulkan.h
    ${ImGuiDir}/backends/imgui_impl_sdl3.cpp
    ${ImGuiDir}/backends/imgui_impl_sdl3.h
  )
  file(GLOB ImGui ${ImGuiDir}/*.cpp ${ImGuiDir}/*.h)
  file(GLOB Volk ${VolkDir}/*.c ${VolkDir}/*.h)
  file(GLOB_RECURSE XenonRender Xenon/Render/*.cpp Xenon/Render/*.h)
  set(Render
    ${ImGuiBackends}
    ${ImGui}
    ${volk}
    ${XenonRender}
  )
endif()

file(GLOB_RECURSE include Xenon/include/*.cpp Xenon/include/*.h)

add_compile_definitions(MICROPROFILE_USE_CONFIG)
set(microprofile_dir ${XENON_THIRDPARTY_DIR}/microprofile)
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
  target_link_libraries(Xenon PRIVATE glad sirit SDL3::SDL3 VulkanMemoryAllocator)
endif()

# Includes
target_include_directories(Xenon PRIVATE
  ${microprofile_dir}
  Xenon/include
  Xenon
)
if (GFX_ENABLED)
  target_include_directories(Xenon SYSTEM PRIVATE
    ${ImGuiDir}
    ${VolkDir}
    ${VkHdrsDir}/include
    ${VMADir}/include
    ${XENON_THIRDPARTY_DIR}/glad
    ${XENON_THIRDPARTY_DIR}/SDL3/include
    ${XENON_THIRDPARTY_DIR}/Sirit/include
    ${XENON_THIRDPARTY_DIR}/Vulkan-Headers/include
    ${XENON_THIRDPARTY_DIR}/VulkanMemoryAllocator/include
    ${XENON_THIRDPARTY_DIR}/volk/include
  )
endif()
target_include_directories(Xenon SYSTEM PRIVATE
  ${XENON_THIRDPARTY_DIR}/fmt/include
  ${XENON_THIRDPARTY_DIR}/toml11
  ${XENON_THIRDPARTY_DIR}/asmjit/src
)

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
