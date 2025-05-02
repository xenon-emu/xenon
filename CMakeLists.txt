# Copyright 2025 Xenon Emulator Project

cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Optimizations
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
if (WIN32)
  add_compile_options($<$<CONFIG:Release>:/Oi>)
  add_compile_options($<$<CONFIG:Release>:/Ot>)
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
endif()

find_package(fmt 10.2.1 CONFIG)
find_package(toml11 4.4.0 CONFIG)

include_directories(Xenon)
add_subdirectory(Deps/ThirdParty)

# Include Files
set(Logging
  Xenon/Base/Logging/Backend.cpp
  Xenon/Base/Logging/Backend.h
  Xenon/Base/Logging/Filter.cpp
  Xenon/Base/Logging/Filter.h
  Xenon/Base/Logging/Log.h
  Xenon/Base/Logging/Log_entry.h
  Xenon/Base/Logging/Log_types.h
  Xenon/Base/Logging/Text_formatter.cpp
  Xenon/Base/Logging/Text_formatter.h
)

set(Base
  ${Logging}
  Xenon/Base/Arch.h
  Xenon/Base/Assert.cpp
  Xenon/Base/Assert.h
  Xenon/Base/Bitfield.h
  Xenon/Base/Bounded_threadsafe_queue.h
  Xenon/Base/Config.cpp
  Xenon/Base/Config.h
  Xenon/Base/Error.cpp
  Xenon/Base/Error.h
  Xenon/Base/Enum.h
  Xenon/Base/Exit.h
  Xenon/Base/Hash.h
  Xenon/Base/io_file.cpp
  Xenon/Base/io_file.h
  Xenon/Base/LRUCache.h
  Xenon/Base/ntapi.cpp
  Xenon/Base/ntapi.h
  Xenon/Base/Param.h
  Xenon/Base/Path_util.cpp
  Xenon/Base/Path_util.h
  Xenon/Base/Polyfill_thread.h
  Xenon/Base/String_util.cpp
  Xenon/Base/String_util.h
  Xenon/Base/SystemDevice.h
  Xenon/Base/Thread.cpp
  Xenon/Base/Thread.h
  Xenon/Base/Types.h
  Xenon/Base/Vector128.h
  Xenon/Base/Version.h
)

set(NAND
  Xenon/Core/NAND/NAND.cpp
  Xenon/Core/NAND/NAND.h
)

set(RAM
  Xenon/Core/RAM/RAM.cpp
  Xenon/Core/RAM/RAM.h
)

set(RootBus
  Xenon/Core/RootBus/RootBus.cpp
  Xenon/Core/RootBus/RootBus.h
  Xenon/Core/RootBus/HostBridge/HostBridge.cpp
  Xenon/Core/RootBus/HostBridge/HostBridge.h
  Xenon/Core/RootBus/HostBridge/PCIe.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/AUDIOCTRLLR/AudioController.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/AUDIOCTRLLR/AudioController.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/EHCI/EHCI.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/EHCI/EHCI.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/EHCI/EHCI0.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/EHCI/EHCI0.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/EHCI/EHCI1.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/EHCI/EHCI1.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/ETHERNET/Ethernet.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/ETHERNET/Ethernet.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/HDD/HDD.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/HDD/HDD.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/ODD/ODD.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/ODD/ODD.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/OHCI/OHCI.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/OHCI/OHCI.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/OHCI/OHCI0.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/OHCI/OHCI0.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/OHCI/OHCI1.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/OHCI/OHCI1.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/PCIBridge.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/PCIBridge.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/PCIBridgeConfig.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/PCIDevice.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/SFCX/SFCX.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/SFCX/SFCX.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/SATA.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/SMC/SMC.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/SMC/SMC.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/SMC/HANA_State.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/SMC/SMC_Config.h
  Xenon/Core/RootBus/HostBridge/PCIBridge/XMA/XMA.cpp
  Xenon/Core/RootBus/HostBridge/PCIBridge/XMA/XMA.h
)

set(XCPU
  Xenon/Core/XCPU/Xenon.cpp
  Xenon/Core/XCPU/Xenon.h
  Xenon/Core/XCPU/XenonSOC.h
  Xenon/Core/XCPU/XenonSOC.cpp
  Xenon/Core/XCPU/XenonReservations.cpp
  Xenon/Core/XCPU/XenonReservations.h
  Xenon/Core/XCPU/eFuse.h
  Xenon/Core/XCPU/UART.cpp
  Xenon/Core/XCPU/UART.h
  Xenon/Core/XCPU/elf_abi.h
  Xenon/Core/XCPU/IIC/IIC.cpp
  Xenon/Core/XCPU/IIC/IIC.h
  Xenon/Core/XCPU/Interpreter/PPC_ALU.cpp
  Xenon/Core/XCPU/Interpreter/PPC_FPU.cpp
  Xenon/Core/XCPU/Interpreter/PPC_VXU.cpp
  Xenon/Core/XCPU/Interpreter/Interpreter_Helpers.cpp
  Xenon/Core/XCPU/Interpreter/PPC_MMU.cpp
  Xenon/Core/XCPU/Interpreter/PPC_Instruction.cpp
  Xenon/Core/XCPU/Interpreter/PPC_Instruction.h
  Xenon/Core/XCPU/Interpreter/PPC_LS.cpp
  Xenon/Core/XCPU/Interpreter/PPC_BC.cpp
  Xenon/Core/XCPU/Interpreter/PPC_System.cpp
  Xenon/Core/XCPU/Interpreter/PPCInterpreter.cpp
  Xenon/Core/XCPU/Interpreter/PPCInterpreter.h
  Xenon/Core/XCPU/Interpreter/PPCInternal.h
  Xenon/Core/XCPU/Interpreter/PPCOpcodes.h
  Xenon/Core/XCPU/PostBus/PostBus.cpp
  Xenon/Core/XCPU/PostBus/PostBus.h
  Xenon/Core/XCPU/PPU/PPU.cpp
  Xenon/Core/XCPU/PPU/PPU.h
  Xenon/Core/XCPU/PPU/PowerPC.h
)

if (GFX_ENABLED)
  set(ImGui_Dir Deps/ThirdParty/ImGui)
  set(ImGui
    ${ImGui_Dir}/backends/imgui_impl_opengl3.cpp
    ${ImGui_Dir}/backends/imgui_impl_opengl3.h
    ${ImGui_Dir}/backends/imgui_impl_sdl3.cpp
    ${ImGui_Dir}/backends/imgui_impl_sdl3.h
    ${ImGui_Dir}/imconfig.h
    ${ImGui_Dir}/imgui.cpp
    ${ImGui_Dir}/imgui.h
    ${ImGui_Dir}/imgui_demo.cpp
    ${ImGui_Dir}/imgui_internal.h
    ${ImGui_Dir}/imgui_draw.cpp
    ${ImGui_Dir}/imgui_tables.cpp
    ${ImGui_Dir}/imgui_widgets.cpp
    ${ImGui_Dir}/imstb_rectpack.h
    ${ImGui_Dir}/imstb_textedit.h
    ${ImGui_Dir}/imstb_truetype.h
  )

  set(Render
    ${ImGui}
    Xenon/Render/Abstractions/Factory/ResourceFactory.h
    Xenon/Render/Abstractions/Factory/ShaderFactory.h
    Xenon/Render/Abstractions/Buffer.h
    Xenon/Render/Abstractions/Renderer.cpp
    Xenon/Render/Abstractions/Renderer.h
    Xenon/Render/Abstractions/Shader.h
    Xenon/Render/Abstractions/Texture.h
    Xenon/Render/GUI/GUI.cpp
    Xenon/Render/GUI/GUI.h
    Xenon/Render/GUI/OpenGL.cpp
    Xenon/Render/GUI/OpenGL.h
    Xenon/Render/OpenGL/Factory/OGLResourceFactory.h
    Xenon/Render/OpenGL/Factory/OGLShaderFactory.cpp
    Xenon/Render/OpenGL/Factory/OGLShaderFactory.h
    Xenon/Render/OpenGL/OGLTexture.cpp
    Xenon/Render/OpenGL/OGLTexture.h
    Xenon/Render/OpenGL/OGLBuffer.cpp
    Xenon/Render/OpenGL/OGLBuffer.h
    Xenon/Render/OpenGL/OGLShader.cpp
    Xenon/Render/OpenGL/OGLShader.h
    Xenon/Render/OGLRenderer.h
    Xenon/Render/OGLRenderer.cpp
  )
endif()
set(microprofile_dir Deps/ThirdParty/microprofile)
set(microprofile
  Xenon/include/microprofile.config.h
  ${microprofile_dir}/microprofile.cpp
  ${microprofile_dir}/microprofile.h
  ${microprofile_dir}/microprofile_html.h
)

set(XGPU
  Xenon/Core/XGPU/XGPU.cpp
  Xenon/Core/XGPU/XenosRegisters.h
  Xenon/Core/XGPU/XGPU.h
  Xenon/Core/XGPU/XGPUConfig.h
  Xenon/Core/XGPU/CommandProcessor.h
  Xenon/Core/XGPU/CommandProcessor.cpp
  Xenon/Core/XGPU/RingBuffer.h
  Xenon/Core/XGPU/RingBuffer.cpp
)

set(Core
  ${NAND}
  ${RAM}
  ${Render}
  ${RootBus}
  ${XCPU}
  ${XGPU}
  Xenon/Core/Xe_Main.cpp
  Xenon/Core/Xe_Main.h
)

add_executable(Xenon
  ${Base}
  ${Core}
  ${microprofile}
  "Xenon/Main.cpp"
)


# Set compiler ID
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(msvc TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(gcc TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(clang TRUE)
endif()

if (GFX_ENABLED)
  add_compile_definitions(GFX_ENABLED)
else()
  add_compile_definitions(NO_GFX)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  add_compile_definitions(DEBUG_BUILD)
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
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
target_link_libraries(Xenon PRIVATE fmt::fmt toml11::toml11)
if (GFX_ENABLED)
  target_link_libraries(Xenon PRIVATE glad SDL3::SDL3)
endif()

# Add includes
target_include_directories(Xenon PRIVATE ${microprofile_dir} Xenon/include)
if (GFX_ENABLED)
    target_include_directories(Xenon PRIVATE ${ImGui_Dir})
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
if (msvc)
  set(CMAKE_GENERATOR_PLATFORM x64)
  set(CMAKE_SYSTEM_VERSION 10.0.19041.0)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()
