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
};

layout(push_constant) uniform PushConstants {
    mat4 model;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragTangent;
layout(location = 4) out vec3 fragBitangent;

void main() {
    vec4 worldPos = model * vec4(inPosition, 1.0);
    gl_Position = viewProj * worldPos;

    mat3 normalMat = mat3(model);
    fragWorldPos = worldPos.xyz;
    fragNormal = normalize(normalMat * inNormal);
    fragTangent = normalize(normalMat * inTangent.xyz);
    fragBitangent = cross(fragNormal, fragTangent) * inTangent.w;
    fragUV = inUV;
}
