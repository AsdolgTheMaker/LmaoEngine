#version 460

layout(push_constant) uniform ShadowPC {
    mat4 mvp;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inTangent;

void main() {
    gl_Position = mvp * vec4(inPosition, 1.0);
}
