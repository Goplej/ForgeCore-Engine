#pragma once
// GLSL 130 sources for the ForgeCore GL backend.
// The lit fragment shader implements the identical lighting model as
// fc::light_fragment() in include/fc/render.hpp (used by the CPU backend),
// so both backends produce matching output.

namespace fc::glsl {

constexpr const char* kLitVertex = R"GLSL(
#version 130
in vec3 aPos;
in vec3 aNormal;
in vec2 aUV;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
void main() {
  vec4 wp = uModel * vec4(aPos, 1.0);
  vWorldPos = wp.xyz;
  vNormal = mat3(uModel) * aNormal;
  vUV = aUV;
  gl_Position = uProj * uView * wp;
}
)GLSL";

constexpr const char* kLitFragment = R"GLSL(
#version 130
precision highp float;
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
uniform vec4 uBase;
uniform vec4 uEmissive;
uniform float uShininess;
uniform float uSpecular;
uniform vec3 uEye;
uniform vec3 uDirDir;
uniform vec3 uDirColor;
uniform vec3 uPointPos[4];
uniform vec3 uPointColor[4];
uniform float uPointRadius[4];
uniform int uNumPoints;
uniform sampler2D uTex;
out vec4 outColor;

vec3 light_fragment(vec3 N, vec3 V, vec3 P) {
  vec3 col = vec3(0.12, 0.13, 0.16) * uBase.rgb;  // ambient fill
  vec3 dir = -normalize(uDirDir);
  float ndl = max(0.0, dot(N, dir));
  vec3 H = normalize(dir + V);
  float spec = pow(max(0.0, dot(N, H)), uShininess) * uSpecular;
  col += uDirColor * (ndl * uBase.rgb + spec);
  for (int i = 0; i < 4; ++i) {
    if (i >= uNumPoints) break;
    vec3 toL = uPointPos[i] - P;
    float d = length(toL);
    float atten = max(0.0, 1.0 - d / max(uPointRadius[i], 0.001));
    atten *= atten;
    vec3 ld = d > 0.001 ? toL / d : vec3(0.0, 1.0, 0.0);
    float ndl2 = max(0.0, dot(N, ld));
    vec3 h2 = normalize(ld + V);
    float spec2 = pow(max(0.0, dot(N, h2)), uShininess) * uSpecular;
    col += uPointColor[i] * atten * (ndl2 * uBase.rgb + spec2);
  }
  return col + uEmissive.rgb;
}

void main() {
  vec3 N = normalize(vNormal);
  vec3 V = normalize(uEye - vWorldPos);
  vec3 lit = light_fragment(N, V, vWorldPos);
  vec4 tex = texture2D(uTex, vUV);
  outColor = vec4(lit * tex.rgb, uBase.a * tex.a);
}
)GLSL";

constexpr const char* kUnlitVertex = R"GLSL(
#version 130
in vec3 aPos;
in vec3 aNormal;
in vec2 aUV;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
out vec2 vUV;
void main() {
  vUV = aUV;
  gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
)GLSL";

constexpr const char* kUnlitFragment = R"GLSL(
#version 130
precision highp float;
in vec2 vUV;
uniform vec4 uBase;
uniform sampler2D uTex;
out vec4 outColor;
void main() {
  vec4 tex = texture2D(uTex, vUV);
  outColor = uBase * tex;
}
)GLSL";

}  // namespace fc::glsl
