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
    vec4 cameraPos;
    float time;
    uint pointLightCount;
    float _pad0;
    float _pad1;
    vec4 dirLightDir;
    vec4 dirLightColor;
};

layout(set = 0, binding = 1) readonly buffer PointLightSSBO {
    GPUPointLight lights[];
};

layout(set = 0, binding = 2) uniform sampler2D gAlbedoMetallic;
layout(set = 0, binding = 3) uniform sampler2D gNormalRoughness;
layout(set = 0, binding = 4) uniform sampler2D gDepth;

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

    // Ambient
    vec3 color = 0.03 * albedo;

    // Directional light
    {
        vec3 L = normalize(-dirLightDir.xyz);
        float intensity = dirLightColor.w;
        color += cookTorranceBRDF(N, V, L, albedo, metallic, roughness, dirLightColor.rgb, intensity);
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
