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
};

layout(set = 0, binding = 4) uniform sampler2D gDepth;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec2 outVelocity;

void main() {
    float depth = texture(gDepth, fragUV).r;

    // Reconstruct world position from depth
    vec4 clipPos = vec4(fragUV * 2.0 - 1.0, depth, 1.0);
    clipPos.y = -clipPos.y;
    vec4 worldPos = invViewProj * clipPos;
    worldPos /= worldPos.w;

    // Project into previous frame (unjittered)
    vec4 prevClip = prevViewProj * worldPos;
    vec2 prevNDC = prevClip.xy / prevClip.w;
    vec2 prevUV = vec2(prevNDC.x, -prevNDC.y) * 0.5 + 0.5;

    // Current unjittered UV computed analytically
    // In non-flipped convention: fragUV.y = (1 - jitteredNDC.y) / 2
    // so unjittered = fragUV + (jitterX/w, -jitterY/h)
    vec2 currentUnjitteredUV = fragUV + vec2(jitterX, -jitterY) * vec2(resolution.z, resolution.w);

    outVelocity = prevUV - currentUnjitteredUV;
}
