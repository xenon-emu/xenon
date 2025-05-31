cmake_minimum_required(VERSION 3.22)

set(Project AST)

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

project(${Project})

add_executable(${Project})

file(GLOB Main *.cpp *.h)

# Registers for the XGPU
set(Xenon
  ../../Xenon/Core/XGPU/PM4Opcodes.h
  ../../Xenon/Core/XGPU/ShaderConstants.h
  ../../Xenon/Core/XGPU/Xenos.h 
  ../../Xenon/Core/XGPU/XenosRegisters.h
)

file(GLOB XenonMicrocode ../../Xenon/Core/XGPU/Microcode/*.cpp ../../Xenon/Core/XGPU/Microcode/*.h)

# Basics to get types, param, and hashe algos
set(XenonBase
  ../../Xenon/Base/Assert.cpp
  ../../Xenon/Base/Assert.h
  ../../Xenon/Base/Arch.h
  ../../Xenon/Base/CRCHash.h
  ../../Xenon/Base/Exit.h
  ../../Xenon/Base/Hash.h
  ../../Xenon/Base/Param.h
  ../../Xenon/Base/Types.h
)

set(XenonAbstractions
  ../../Xenon/Render/Abstractions/Factory/ResourceFactory.h
  ../../Xenon/Render/Abstractions/Factory/ShaderFactory.h
  ../../Xenon/Render/Abstractions/Buffer.h
  ../../Xenon/Render/Abstractions/Shader.h
  ../../Xenon/Render/Abstractions/Texture.h
  ../../Xenon/Render/Abstractions/VertexInput.h
)

file(GLOB_RECURSE XenonOpenGL ../../Xenon/Render/OpenGL/*.cpp ../../Xenon/Render/OpenGL/*.h)

file(GLOB_RECURSE XenonDummy ../../Xenon/Render/Dummy/*.cpp ../../Xenon/Render/Dummy/*.h)

# Sets as tool, and disables the logger
add_compile_definitions(TOOL)

target_sources(${Project} PRIVATE
  ${Main}
  ${XenonAbstractions}
  ${XenonDummy}
  ${XenonMicrocode}
  ${XenonOpenGL}
  ${Xenon}
  ${XenonBase}
)

target_precompile_headers(${Project} PRIVATE ../../Xenon/Base/Global.h)

target_link_libraries(${Project} PRIVATE glad sirit SDL3::SDL3)

target_include_directories(${Project} BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../../Xenon
)

add_subdirectory(Deps/ThirdParty)

target_link_options(${Project} PRIVATE -static)
