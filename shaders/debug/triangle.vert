#version 460

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
    float time;
};

layout(push_constant) uniform PushConstants {
    mat4 model;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = model * vec4(inPosition, 1.0);
    gl_Position = viewProj * worldPos;
    fragColor = inColor;
    fragWorldPos = worldPos.xyz;
}
