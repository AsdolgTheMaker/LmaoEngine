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

layout(set = 0, binding = 3) uniform sampler2D gNormalRoughness;
layout(set = 0, binding = 4) uniform sampler2D gDepth;

layout(set = 1, binding = 0) uniform sampler2D noiseTex;
layout(set = 1, binding = 1) uniform SSAOKernel {
    vec4 samples[64];
} kernel;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out float outAO;

// Reconstruct view-space position from UV + depth via inverse(viewProj) then view
vec3 getViewPos(vec2 uv, float d) {
    vec4 clip = vec4(uv * 2.0 - 1.0, d, 1.0);
    clip.y = -clip.y; // Y-flip to match G-buffer viewport
    vec4 world = invViewProj * clip;
    world /= world.w;
    return (view * world).xyz;
}

void main() {
    float depth = texture(gDepth, fragUV).r;
    if (depth < 0.0001) { outAO = 1.0; return; } // sky (reversed-Z: 0 = far)

    // Reconstruct view-space position of this fragment
    vec3 fragViewPos = getViewPos(fragUV, depth);

    // View-space normal
    vec3 worldNormal = normalize(texture(gNormalRoughness, fragUV).rgb * 2.0 - 1.0);
    vec3 viewNormal = normalize(mat3(view) * worldNormal);

    // Noise for random rotation (tile 4x4 noise texture across the half-res screen)
    vec2 noiseScale = resolution.xy * 0.5 / 4.0;
    vec3 randomVec = vec3(texture(noiseTex, fragUV * noiseScale).rg, 0.0);

    // Build TBN from noise + normal (Gramm-Schmidt orthogonalization)
    vec3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    vec3 bitangent = cross(viewNormal, tangent);
    mat3 TBN = mat3(tangent, bitangent, viewNormal);

    float occlusion = 0.0;
    for (int i = 0; i < 64; i++) {
        // Offset sample position in view space (hemisphere oriented along normal)
        vec3 samplePos = fragViewPos + TBN * kernel.samples[i].xyz * ssaoRadius;

        // Project sample back to screen UV
        vec4 sampleClip = proj * vec4(samplePos, 1.0);
        sampleClip.xyz /= sampleClip.w;
        vec2 sampleUV = vec2(sampleClip.x, -sampleClip.y) * 0.5 + 0.5;

        // Get actual depth and view-space position at the sample's screen location
        float actualDepth = texture(gDepth, sampleUV).r;
        vec3 actualViewPos = getViewPos(sampleUV, actualDepth);

        // In RH view space, camera looks down -Z:
        //   closer to camera = less negative z = LARGER z value
        // Occlusion occurs when actual surface is closer to camera than the sample point:
        //   actualViewPos.z > samplePos.z (actual has larger z = closer)
        float depthDiff = actualViewPos.z - samplePos.z;

        // Range check: ignore surfaces too far from the fragment (prevents halo artifacts)
        float rangeCheck = smoothstep(0.0, 1.0, ssaoRadius / (abs(fragViewPos.z - actualViewPos.z) + 0.001));

        // Count as occluded if actual surface is closer by at least 'bias' (prevents self-occlusion)
        occlusion += step(ssaoBias, depthDiff) * rangeCheck;
    }

    outAO = 1.0 - (occlusion / 64.0);
}
