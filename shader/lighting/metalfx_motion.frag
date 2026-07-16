#version 450

layout(binding = 0) uniform sampler2D depthTexture;

layout(push_constant, std430) uniform Push {
  mat4 currentJitteredViewProjectInv;
  mat4 currentViewProject;
  mat4 previousViewProject;
} push;

layout(location = 0) out vec2 outMotion;

void main() {
  const ivec2 size = textureSize(depthTexture, 0);
  const ivec2 at   = ivec2(gl_FragCoord.xy);
  const vec2  uv   = (vec2(at) + vec2(0.5)) / vec2(size);
  const float z    = texelFetch(depthTexture, at, 0).r;

  vec4 world = push.currentJitteredViewProjectInv * vec4(uv * 2.0 - 1.0, z, 1.0);
  if(abs(world.w) < 1e-6) {
    outMotion = vec2(0.0);
    return;
  }
  world /= world.w;

  const vec4 currentClip  = push.currentViewProject  * world;
  const vec4 previousClip = push.previousViewProject * world;
  if(currentClip.w <= 1e-6 || previousClip.w <= 1e-6) {
    outMotion = vec2(0.0);
    return;
  }

  const vec2 currentUv  = currentClip.xy  / currentClip.w  * 0.5 + 0.5;
  const vec2 previousUv = previousClip.xy / previousClip.w * 0.5 + 0.5;

  // MetalFX expects vectors from the current pixel to its previous-frame
  // location. The backend scales these normalized values by the input size.
  outMotion = previousUv - currentUv;
}
