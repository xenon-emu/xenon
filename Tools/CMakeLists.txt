# Copyright 2025 Xenon Emulator Project

cmake_minimum_required(VERSION 3.22)

set(Project XenonTools)

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
else()
  add_compile_options($<$<CONFIG:Release>:-Ofast>)
endif()

# Options
option(GFX_ENABLED "Enable graphics" ON)
option(XENON_USE_SYSTEM_DEPS "Prefer system-installed packages (find_package first)" ON)
option(XENON_ALLOW_BUNDLED_DEPS "If a package isn't found, fall back to bundled subdirs" ON)
set(XENON_THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Deps/ThirdParty" CACHE PATH "Bundled deps root")

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
      endif()

      message(STATUS "Using bundled ${pkg}")
      add_subdirectory(${XENON_THIRDPARTY_DIR}/${subdir} ${subdir} EXCLUDE_FROM_ALL)
    endif()
  endif()
endmacro()

project(${Project})

# Provides a dead-simple logger, designed to avoid threading for tools
file(GLOB_RECURSE SimpleBase ../Base/*.cpp ../Base/*.h)

xenon_dep(sirit sirit Sirit)

# Registers for the XGPU
set(Xenon
  ../Xenon/Core/XGPU/PM4Opcodes.h
  ../Xenon/Core/XGPU/ShaderConstants.h
  ../Xenon/Core/XGPU/Xenos.h
  ../Xenon/Core/XGPU/XenosRegisters.h
)

# Basics to get types, param, and hashe algos
set(XenonBase
  ../Xenon/Base/Logging/Log.h
  ../Xenon/Base/Arch.h
  ../Xenon/Base/CRCHash.h
  ../Xenon/Base/Exit.h
  ../Xenon/Base/Hash.h
  ../Xenon/Base/Param.h
  ../Xenon/Base/Types.h
  ../Xenon/Base/Vector128.h
)

# Sets as tool, and disables the logger
add_compile_definitions(TOOL)

add_executable(Byteswap
  byteswap.cpp
  ${SimpleBase}
  ${XenonBase}
)

target_precompile_headers(Byteswap PRIVATE ../Xenon/Base/Global.h)
target_include_directories(Byteswap PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../Xenon
)

add_executable(GetIndex
  get_idx.cpp
  ${SimpleBase}
  ${XenonBase}
)

target_precompile_headers(GetIndex PRIVATE ../Xenon/Base/Global.h)
target_include_directories(GetIndex PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../Xenon
)

add_executable(xenos-disasm
  xenos_disasm.cpp
  ${SimpleBase}
  ${XenonBase}
)

target_precompile_headers(xenos-disasm PRIVATE ../Xenon/Base/Global.h)
target_link_libraries(xenos-disasm PRIVATE sirit)
target_include_directories(xenos-disasm PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${XENON_THIRDPARTY_DIR}/Sirit/include
  ../Xenon
)

add_executable(GetOpcode
  get_opcode.cpp
  ${SimpleBase}
  ${XenonBase}
)

target_precompile_headers(GetOpcode PRIVATE ../Xenon/Base/Global.h)
target_include_directories(GetOpcode PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../Xenon
)

# Enable SSE3 for VPUTests
if (gcc OR clang)
  target_compile_options(VPUTests PRIVATE -msse2)
  target_compile_options(VPUTests PRIVATE -mssse3)
elseif (msvc)
  target_compile_options(VPUTests PRIVATE /arch:SSE2)
endif()

add_executable(VPUTests
  vpu_tests.cpp
  ${SimpleBase}
  ${XenonBase}
)

target_precompile_headers(VPUTests PRIVATE ../Xenon/Base/Global.h)
target_include_directories(VPUTests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../Xenon
)
