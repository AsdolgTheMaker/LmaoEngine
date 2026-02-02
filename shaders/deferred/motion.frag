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

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec2 outVelocity;

void main() {
    float depth = texture(gDepth, fragUV).r;

    // Current unjittered UV computed analytically
    vec2 currentUnjitteredUV = fragUV + vec2(jitterX, -jitterY) * vec2(resolution.z, resolution.w);

    if (depth < 0.0001) {
        // Sky pixel: reproject direction (not position) to avoid finite far-plane parallax
        vec2 ndc = vec2(fragUV.x * 2.0 - 1.0, -(fragUV.y * 2.0 - 1.0));
        vec3 viewDir = vec3(ndc.x / proj[0][0], ndc.y / proj[1][1], -1.0);
        vec3 worldDir = transpose(mat3(view)) * viewDir;

        // For infinitely distant point in direction d: NDC = (VP*(d,0)).xy / (VP*(d,0)).w
        vec4 prevClip = prevViewProj * vec4(worldDir, 0.0);
        vec2 prevNDC = prevClip.xy / prevClip.w;
        vec2 prevUV = vec2(prevNDC.x, -prevNDC.y) * 0.5 + 0.5;

        outVelocity = prevUV - currentUnjitteredUV;
        return;
    }

    // Reconstruct world position from depth
    vec4 clipPos = vec4(fragUV * 2.0 - 1.0, depth, 1.0);
    clipPos.y = -clipPos.y;
    vec4 worldPos = invViewProj * clipPos;
    worldPos /= worldPos.w;

    // Project into previous frame (unjittered)
    vec4 prevClip = prevViewProj * worldPos;
    vec2 prevNDC = prevClip.xy / prevClip.w;
    vec2 prevUV = vec2(prevNDC.x, -prevNDC.y) * 0.5 + 0.5;

    outVelocity = prevUV - currentUnjitteredUV;
}
