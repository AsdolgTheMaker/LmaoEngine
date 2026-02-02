#version 460

struct GPUPointLight {
    vec4 positionAndRange;   // xyz = position, w = range
    vec4 colorAndIntensity;  // xyz = color, w = intensity
};

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
    vec4 cascadeSplits; // xyz = split depths (view-space), w = shadow bias
    float iblIntensity;
};

layout(set = 0, binding = 1) readonly buffer PointLightSSBO {
    GPUPointLight lights[];
};

layout(set = 0, binding = 2) uniform sampler2D gAlbedoMetallic;
layout(set = 0, binding = 3) uniform sampler2D gNormalRoughness;
layout(set = 0, binding = 4) uniform sampler2D gDepth;
layout(set = 0, binding = 5) uniform sampler2DArrayShadow shadowMap;
layout(set = 0, binding = 6) uniform samplerCube irradianceMap;
layout(set = 0, binding = 7) uniform samplerCube prefilteredMap;
layout(set = 0, binding = 8) uniform sampler2D brdfLUT;

layout(push_constant) uniform LightingPC {
    uint debugMode;
};

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// GGX/Trowbridge-Reitz NDF
float distributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Smith-Schlick geometry function
float geometrySmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
    return ggx1 * ggx2;
}

// Schlick Fresnel
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Schlick Fresnel with roughness (for IBL ambient)
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Reconstruct world position from depth
vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    // Flip Y back to match projection convention
    clipPos.y = -clipPos.y;
    vec4 worldPos = invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}

vec3 cookTorranceBRDF(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness, vec3 lightColor, float lightIntensity) {
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    if (NdotL <= 0.0) return vec3(0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3 F = fresnelSchlick(HdotV, F0);

    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * lightColor * lightIntensity * NdotL;
}

// Interleaved gradient noise for per-pixel PCF rotation (Jorge Jimenez, 2014)
float interleavedGradientNoise(vec2 pos) {
    return fract(52.9829189 * fract(0.06711056 * pos.x + 0.00583715 * pos.y));
}

// 16-tap Poisson disk for soft shadow sampling
const vec2 poissonDisk[16] = vec2[](
    vec2(-0.9420, -0.3991), vec2( 0.9456, -0.7689),
    vec2(-0.0942, -0.9294), vec2( 0.3450,  0.2939),
    vec2(-0.9159,  0.4577), vec2(-0.8154, -0.8791),
    vec2(-0.3828,  0.2768), vec2( 0.9748,  0.7565),
    vec2( 0.4432, -0.9751), vec2( 0.5374, -0.4737),
    vec2(-0.2650, -0.4189), vec2( 0.7920,  0.1909),
    vec2(-0.2419,  0.9971), vec2(-0.8141,  0.9144),
    vec2( 0.1998,  0.7864), vec2( 0.1438, -0.1410)
);

float sampleShadowCascade(vec3 worldPos, int cascadeIdx) {
    vec4 shadowCoord = cascadeViewProj[cascadeIdx] * vec4(worldPos, 1.0);
    shadowCoord.xyz /= shadowCoord.w;
    vec2 shadowUV = shadowCoord.xy * 0.5 + 0.5;

    if (any(lessThan(shadowUV, vec2(0.0))) || any(greaterThan(shadowUV, vec2(1.0)))) {
        return 1.0;
    }

    float compareDepth = shadowCoord.z;
    float texelSize = 1.0 / float(textureSize(shadowMap, 0).x);

    // Per-pixel rotation angle from interleaved gradient noise
    float angle = interleavedGradientNoise(gl_FragCoord.xy) * 6.283185;
    float sa = sin(angle);
    float ca = cos(angle);
    mat2 rotation = mat2(ca, sa, -sa, ca);

    // Spread radius: larger for far cascades (maintains consistent penumbra in world space)
    float spread = texelSize * (1.5 + float(cascadeIdx) * 0.5);

    float shadow = 0.0;
    for (int i = 0; i < 16; i++) {
        vec2 offset = rotation * poissonDisk[i] * spread;
        shadow += texture(shadowMap, vec4(shadowUV + offset, float(cascadeIdx), compareDepth));
    }
    return shadow / 16.0;
}

float sampleShadowPCF(vec3 worldPos, float viewZ) {
    int cascadeIdx = 0;
    if (viewZ > cascadeSplits.x) cascadeIdx = 1;
    if (viewZ > cascadeSplits.y) cascadeIdx = 2;

    float shadow = sampleShadowCascade(worldPos, cascadeIdx);

    // Blend between cascades at boundaries for smooth transitions
    float blendRange = 0.1; // 10% of cascade range as blend zone
    if (cascadeIdx < 2) {
        float splitDist = (cascadeIdx == 0) ? cascadeSplits.x : cascadeSplits.y;
        float fade = clamp((viewZ - splitDist * (1.0 - blendRange)) / (splitDist * blendRange), 0.0, 1.0);
        if (fade > 0.0) {
            float nextShadow = sampleShadowCascade(worldPos, cascadeIdx + 1);
            shadow = mix(shadow, nextShadow, fade);
        }
    }

    return shadow;
}

// Debug: cascade index visualization
vec3 cascadeDebugColor(float viewZ) {
    if (viewZ <= cascadeSplits.x) return vec3(1.0, 0.2, 0.2);
    if (viewZ <= cascadeSplits.y) return vec3(0.2, 1.0, 0.2);
    return vec3(0.2, 0.2, 1.0);
}

void main() {
    // Sample G-buffer
    vec4 albedoMetallic = texture(gAlbedoMetallic, fragUV);
    vec4 normalRoughness = texture(gNormalRoughness, fragUV);
    float depth = texture(gDepth, fragUV).r;

    vec3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 N = normalize(normalRoughness.rgb * 2.0 - 1.0);
    float roughness = normalRoughness.a;

    // Debug visualization modes
    if (debugMode == 1u) { outColor = vec4(albedo, 1.0); return; }
    if (debugMode == 2u) { outColor = vec4(vec3(metallic), 1.0); return; }
    if (debugMode == 3u) { outColor = vec4(vec3(roughness), 1.0); return; }
    if (debugMode == 4u) { outColor = vec4(N * 0.5 + 0.5, 1.0); return; }
    if (debugMode == 5u) { outColor = vec4(vec3(depth), 1.0); return; }

    // Reconstruct world position from depth
    vec3 worldPos = reconstructWorldPos(fragUV, depth);
    vec3 V = normalize(cameraPos.xyz - worldPos);

    // View-space depth for cascade selection
    float viewZ = -(view * vec4(worldPos, 1.0)).z;

    // Cascade debug mode
    if (debugMode == 7u) {
        vec3 baseColor = cascadeDebugColor(viewZ);
        float shadow = sampleShadowPCF(worldPos, viewZ);
        outColor = vec4(baseColor * (shadow * 0.7 + 0.3), 1.0);
        return;
    }

    // IBL ambient (split-sum approximation)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NdotV = max(dot(N, V), 0.0);
    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);

    // Diffuse IBL
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL = kD * albedo * irradiance;

    // Specular IBL
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 R = reflect(-V, N);
    vec3 prefilteredColor = textureLod(prefilteredMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

    vec3 color = (diffuseIBL + specularIBL) * iblIntensity;

    // Shadow factor for directional light
    float shadow = (depth == 0.0) ? 1.0 : sampleShadowPCF(worldPos, viewZ);

    // Directional light
    {
        vec3 L = normalize(-dirLightDir.xyz);
        float intensity = dirLightColor.w;
        color += shadow * cookTorranceBRDF(N, V, L, albedo, metallic, roughness, dirLightColor.rgb, intensity);
    }

    // Point lights
    for (uint i = 0; i < pointLightCount; i++) {
        vec3 lightPos = lights[i].positionAndRange.xyz;
        float range = lights[i].positionAndRange.w;
        vec3 lightColor = lights[i].colorAndIntensity.xyz;
        float intensity = lights[i].colorAndIntensity.w;

        vec3 toLight = lightPos - worldPos;
        float dist = length(toLight);

        if (dist >= range) continue;

        vec3 L = toLight / dist;
        float attenuation = clamp(1.0 - dist / range, 0.0, 1.0);
        attenuation *= attenuation;

        color += cookTorranceBRDF(N, V, L, albedo, metallic, roughness, lightColor, intensity * attenuation);
    }

    outColor = vec4(color, 1.0);
}
