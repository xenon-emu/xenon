cmake_minimum_required(VERSION 3.22)

set(projectname AST)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

project(${projectname})

add_executable(${projectname})

target_sources(${projectname} PRIVATE
  Base/Logging/Log.h
  Base/CRCHash.h
  Base/Hash.h
  Base/Param.h
  Base/Types.h
  Microcode/ASTBlock.cpp
  Microcode/ASTBlock.h
  Microcode/ASTEmitter.cpp
  Microcode/ASTEmitter.h
  Microcode/ASTNode.cpp
  Microcode/ASTNode.h
  Microcode/ASTNodeBase.h
  Microcode/ASTNodeWriter.cpp
  Microcode/ASTNodeWriter.h
  Microcode/ASTStatement.cpp
  Microcode/ASTStatement.h
  Microcode/Constants.h
  Microcode/Transformer.cpp
  Microcode/Transformer.h
  main.cpp
  ShaderConstants.h
  XenosRegisters.h)

target_link_libraries(${projectname} PRIVATE sirit)

target_include_directories(${projectname} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  Base
)

add_subdirectory(Deps/ThirdParty)

target_link_options(${projectname} PRIVATE -static)
