#pragma once

#if __has_include(<glslang/SPIRV/GlslangToSpv.h>)
  #include <glslang/SPIRV/GlslangToSpv.h>
#elif __has_include(<SPIRV/GlslangToSpv.h>)
  #include <SPIRV/GlslangToSpv.h>
#else
  #error "Missing glslang GlslangToSpv.h"
#endif