#version 460

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invViewProj;
    mat4 prevViewProj;
    vec4 cameraPos;
    float time;
    uint pointLightCount;
    float jitterX;
    float jitterY;
    vec4 dirLightDir;
    vec4 dirLightColor;
    vec4 resolution;
    mat4 cascadeViewProj[3];
    vec4 cascadeSplits;
    float iblIntensity;
    float ssaoRadius;
    float ssaoBias;
    float bloomIntensity;
};

layout(set = 0, binding = 4) uniform sampler2D gDepth;
layout(set = 1, binding = 0) uniform samplerCube envMap;

layout(location = 0) in vec3 fragDir;

layout(location = 0) out vec4 outColor;

void main() {
    // Only draw sky pixels (reversed-Z: depth == 0 is far plane)
    vec2 uv = gl_FragCoord.xy * resolution.zw;
    float depth = texture(gDepth, uv).r;
    if (depth > 0.0001) discard;

    outColor = vec4(texture(envMap, normalize(fragDir)).rgb, 1.0);
}
