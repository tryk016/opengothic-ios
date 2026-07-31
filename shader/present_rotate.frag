#version 450
#extension GL_ARB_separate_shader_objects : enable

// Android pre-rotation. The engine renders everything - scene, UI, inventory -
// into a landscape target exactly as before; this pass is the only place that
// knows the swapchain image is portrait. Doing the rotation here is what lets
// SurfaceFlinger scan the buffer out directly (DEVICE composition) instead of
// running its own full-screen GPU composition pass on the same Mali for 98.9%
// of frames. See docs/superpowers/reports/2026-07-31-frame-budget-decomposition.md
//
// gl_FragCoord is in destination (portrait) pixels; src is landscape.
// Direction is settable because which of the two 90-degree rotations is correct
// depends on how the panel's native orientation maps to the window, and that is
// cheaper to establish with one on-device build than to reason about.

layout(binding = 0) uniform sampler2D src;

layout(push_constant, std430) uniform Push {
  int ccw;
  } push;

layout(location = 0) out vec4 outColor;

void main() {
  const ivec2 dst = ivec2(gl_FragCoord.xy);
  const ivec2 sz  = textureSize(src, 0);

  ivec2 uv;
  if(push.ccw!=0)
    uv = ivec2(sz.x - 1 - dst.y, dst.x); else
    uv = ivec2(dst.y, sz.y - 1 - dst.x);

  outColor = texelFetch(src, clamp(uv, ivec2(0), sz - ivec2(1)), 0);
  }
