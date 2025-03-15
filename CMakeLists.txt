# Copyright 2025 Xenon Emulator Project

cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

project(Xenon)

if (NOT GFX_ENABLED)
  set(GFX_ENABLED True)
endif()

# This function should be passed a list of all files in a target. It will automatically generate file groups
# following the directory hierarchy, so that the layout of the files in IDEs matches the one in the filesystem.
function(create_target_directory_groups target_name)
  # Place any files that aren't in the source list in a separate group so that they don't get in the way.
  source_group("Other Files" REGULAR_EXPRESSION ".")
  get_target_property(target_sources "${target_name}" SOURCES)
  foreach(file_name IN LISTS target_sources)
    get_filename_component(dir_name "${file_name}" PATH)
    # Group names use '\' as a separator even though the entire rest of CMake uses '/'...
    string(REPLACE "/" "\\" group_name "${dir_name}")
    source_group("${group_name}" FILES "${file_name}")
  endforeach()
endfunction()

find_package(fmt 10.2.1 CONFIG)
if (GFX_ENABLED STREQUAL "True")
  find_package(SDL3 3.2.4 CONFIG)
endif()
find_package(toml11 3.7.1 CONFIG)

add_subdirectory(third_party)

set(Logging
    Xenon/Base/Logging/Backend.cpp
    Xenon/Base/Logging/Backend.h
    Xenon/Base/Logging/Filter.cpp
    Xenon/Base/Logging/Filter.h
    Xenon/Base/Logging/Formatter.h
    Xenon/Base/Logging/Log.h
    Xenon/Base/Logging/Log_entry.h
    Xenon/Base/Logging/Text_formatter.cpp
    Xenon/Base/Logging/Text_formatter.h
    Xenon/Base/Logging/Types.h
)

set(Base
    ${Logging}
    Xenon/Base/Alignment.h
    Xenon/Base/Arch.h
    Xenon/Base/Assert.cpp
    Xenon/Base/Assert.h
    Xenon/Base/Bounded_threadsafe_queue.h
    Xenon/Base/Concepts.h
    Xenon/Base/Config.cpp
    Xenon/Base/Config.h
    Xenon/Base/Error.cpp
    Xenon/Base/Error.h
    Xenon/Base/Enum.h
    Xenon/Base/io_file.cpp
    Xenon/Base/io_file.h
    Xenon/Base/LRUCache.h
    Xenon/Base/ntapi.cpp
    Xenon/Base/ntapi.h
    Xenon/Base/Path_util.cpp
    Xenon/Base/Path_util.h
    Xenon/Base/Polyfill_thread.h
    Xenon/Base/String_util.cpp
    Xenon/Base/String_util.h
    Xenon/Base/SystemDevice.h
    Xenon/Base/Thread.cpp
    Xenon/Base/Thread.h
    Xenon/Base/Types.h
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
    Xenon/Core/RootBus/HostBridge/PCIBridge/EHCI0/EHCI0.cpp
    Xenon/Core/RootBus/HostBridge/PCIBridge/EHCI0/EHCI0.h
    Xenon/Core/RootBus/HostBridge/PCIBridge/EHCI1/EHCI1.cpp
    Xenon/Core/RootBus/HostBridge/PCIBridge/EHCI1/EHCI1.h
    Xenon/Core/RootBus/HostBridge/PCIBridge/ETHERNET/Ethernet.cpp
    Xenon/Core/RootBus/HostBridge/PCIBridge/ETHERNET/Ethernet.h
    Xenon/Core/RootBus/HostBridge/PCIBridge/HDD/HDD.cpp
    Xenon/Core/RootBus/HostBridge/PCIBridge/HDD/HDD.h
    Xenon/Core/RootBus/HostBridge/PCIBridge/ODD/ODD.cpp
    Xenon/Core/RootBus/HostBridge/PCIBridge/ODD/ODD.h
    Xenon/Core/RootBus/HostBridge/PCIBridge/OHCI0/OHCI0.cpp
    Xenon/Core/RootBus/HostBridge/PCIBridge/OHCI0/OHCI0.h
    Xenon/Core/RootBus/HostBridge/PCIBridge/OHCI1/OHCI1.cpp
    Xenon/Core/RootBus/HostBridge/PCIBridge/OHCI1/OHCI1.h
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
    Xenon/Core/XCPU/Bitfield.h
    Xenon/Core/XCPU/Xenon.cpp
    Xenon/Core/XCPU/Xenon.h
    Xenon/Core/XCPU/XenonReservations.cpp
    Xenon/Core/XCPU/XenonReservations.h
    Xenon/Core/XCPU/eFuse.h
    Xenon/Core/XCPU/elf_abi.h
    Xenon/Core/XCPU/IIC/IIC.cpp
    Xenon/Core/XCPU/IIC/IIC.h
    Xenon/Core/XCPU/Interpreter/PPC_ALU.cpp
    Xenon/Core/XCPU/Interpreter/PPC_FPU.cpp
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

if (GFX_ENABLED STREQUAL "True")
  set(IMGUI_DIR third_party/ImGui)
  set(IMGUI
      ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
      ${IMGUI_DIR}/backends/imgui_impl_opengl3.h
      ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
      ${IMGUI_DIR}/backends/imgui_impl_sdl3.h
      ${IMGUI_DIR}/imconfig.h
      ${IMGUI_DIR}/imgui.cpp
      ${IMGUI_DIR}/imgui.h
      ${IMGUI_DIR}/imgui_demo.cpp
      ${IMGUI_DIR}/imgui_internal.h
      ${IMGUI_DIR}/imgui_draw.cpp
      ${IMGUI_DIR}/imgui_tables.cpp
      ${IMGUI_DIR}/imgui_widgets.cpp
      ${IMGUI_DIR}/imstb_rectpack.h
      ${IMGUI_DIR}/imstb_textedit.h
      ${IMGUI_DIR}/imstb_truetype.h
  )

  set(OPENGL
      third_party/glad/src/glad.c
  )

  set(RENDER
      ${OPENGL}
      ${IMGUI}
      Xenon/Render/Abstractions/Texture.h
      Xenon/Render/Implementations/OGLTexture.cpp
      Xenon/Render/Implementations/OGLTexture.h
      Xenon/Render/GUI/Implementations/OpenGL.cpp
      Xenon/Render/GUI/Implementations/OpenGL.h
      Xenon/Render/GUI/GUI.cpp
      Xenon/Render/GUI/GUI.h
      Xenon/Render/Renderer.cpp
      Xenon/Render/Renderer.h
  )
endif()

set(XGPU
    Xenon/Core/XGPU/XGPU.cpp
    Xenon/Core/XGPU/XenosRegisters.h
    Xenon/Core/XGPU/XGPU.h
    Xenon/Core/XGPU/XGPUConfig.h
)

set(Core
    ${NAND}
    ${RAM}
    ${RENDER}
    ${RootBus}
    ${XCPU}
    ${XGPU}
    Xenon/Core/Xe_Main.cpp
    Xenon/Core/Xe_Main.h
)

add_executable(Xenon
    ${Base}
    ${Core}
    Xenon/Main.cpp
)

create_target_directory_groups(Xenon)

# Test compiler ID
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(msvc TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(gcc TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(clang TRUE)
endif()

# Copy fonts to build directory
if (GFX_ENABLED STREQUAL "True")
  add_custom_target(copy-fonts
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Deps/fonts" # Input
            "${CMAKE_CURRENT_BINARY_DIR}/fonts"      # Output
    COMMENT "Copying fonts (${CMAKE_CURRENT_SOURCE_DIR}/Deps/fonts) to build directory (${CMAKE_CURRENT_BINARY_DIR}/fonts)"
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Deps/fonts
  )
endif()

# Include OPENGL Directory
include_directories(Xenon third_party/glad/include)

if (GFX_ENABLED STREQUAL "True")
  add_dependencies(Xenon copy-fonts)
endif()
target_include_directories(Xenon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${IMGUI_DIR})
if (GFX_ENABLED STREQUAL "True")
  target_link_libraries(Xenon PRIVATE fmt::fmt SDL3::SDL3 toml11::toml11)
else()
  target_link_libraries(Xenon PRIVATE fmt::fmt toml11::toml11)
endif()

add_definitions(-DNTDDI_VERSION=0x0A000006 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00)
add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
if (GFX_ENABLED STREQUAL "False")
  add_definitions(-DNO_GFX)
endif()
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

if (msvc)
  set(CMAKE_GENERATOR_PLATFORM x64)
  set(CMAKE_SYSTEM_VERSION 10.0.19041.0)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

if (WIN32)
  target_sources(Xenon PRIVATE Xenon/Xenon.rc)
endif()