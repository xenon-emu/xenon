/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#ifndef NO_GFX

namespace Render::Vulkan {

inline constexpr const char vertexShaderSource[] = R"glsl(#version 450 core
layout(location = 0) out vec2 o_texture_coord;

void main() {
  o_texture_coord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(o_texture_coord * 2.0f + vec2(-1.0f), 0.0f, 1.0f);
})glsl";

inline constexpr const char fragmentShaderSource[] = R"glsl(#version 450 core

layout(location = 0) in vec2 i_texture_coord;
layout(location = 0) out vec4 o_color;

layout(set = 0, binding = 0) uniform usampler2D u_texture;

void main() {
  uint pixel = texture(u_texture, i_texture_coord).r;
  // Gotta love BE vs LE (X360 works in BGRA, so we work in ARGB)
  float a = float((pixel >> 24u) & 0xFFu) / 255.0;
  float r = float((pixel >> 16u) & 0xFFu) / 255.0;
  float g = float((pixel >> 8u) & 0xFFu) / 255.0;
  float b = float((pixel >> 0u) & 0xFFu) / 255.0;
  o_color = vec4(r, g, b, a);
})glsl";

inline constexpr const char computeShaderSource[] = R"glsl(#version 450 core

layout (local_size_x = 16, local_size_y = 16) in;

layout (r32ui, binding = 0) uniform writeonly uimage2D o_texture;
layout (std430, binding = 1) buffer pixel_buffer {
  uint pixel_data[];
};

layout(push_constant) uniform Push {
  int internalWidth;
  int internalHeight;
  int resWidth;
  int resHeight;
} pc;

// This is black magic to convert tiles to linear, just don't touch it
int xeFbConvert(int width, int addr) {
  int y = addr / (width * 4);
  int x = (addr % (width * 4)) / 4;
  return ((((y & ~31) * width) + (x & ~31) * 32) +
         (((x & 3) + ((y & 1) << 2) + ((x & 28) << 1) + ((y & 30) << 5)) ^
         ((y & 8) << 2)));
}

void main() {
  ivec2 texel_pos = ivec2(gl_GlobalInvocationID.xy);
  // OOB check, but shouldn't be needed
  if (texel_pos.x >= pc.resWidth || texel_pos.y >= pc.resHeight)
    return;

  // Scale accordingly
  float scaleX = float(pc.internalWidth) / float(pc.resWidth);
  float scaleY = float(pc.internalHeight) / float(pc.resHeight);

  // Map to source resolution
  int srcX = int(float(texel_pos.x) * scaleX);
  int srcY = int(float(texel_pos.y) * scaleY);

  // God only knows how this indexing works
  int stdIndex = (srcY * pc.internalWidth + srcX);
  int xeIndex = xeFbConvert(pc.internalWidth, stdIndex * 4);

  uint packedColor = pixel_data[xeIndex];
  imageStore(o_texture, texel_pos, uvec4(packedColor, 0, 0, 0));
})glsl";

} // namespace Render

#endif // ifndef NO_GFX