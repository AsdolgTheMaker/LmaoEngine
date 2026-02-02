#version 460

layout(set = 2, binding = 0) uniform sampler2D albedoMap;
layout(set = 2, binding = 1) uniform sampler2D normalMap;
layout(set = 2, binding = 2) uniform sampler2D metalRoughMap;
layout(set = 2, binding = 3) uniform MaterialUBO {
    vec4 albedoColor;
    float metallic;
    float roughness;
    float normalScale;
    float _matPad;
};

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec3 fragBitangent;

layout(location = 0) out vec4 outAlbedoMetallic;    // RT0: RGB=albedo, A=metallic
layout(location = 1) out vec4 outNormalRoughness;    // RT1: RGB=world normal, A=roughness

void main() {
    // Sample textures
    vec4 albedoSample = texture(albedoMap, fragUV);
    vec3 albedo = albedoSample.rgb * albedoColor.rgb;

    vec4 mrSample = texture(metalRoughMap, fragUV);
    float finalMetallic = mrSample.b * metallic;
    float finalRoughness = mrSample.g * roughness;

    // Normal mapping
    vec3 N = normalize(fragNormal);
    vec3 T = normalize(fragTangent);
    vec3 B = normalize(fragBitangent);
    mat3 TBN = mat3(T, B, N);

    vec3 tangentNormal = texture(normalMap, fragUV).xyz * 2.0 - 1.0;
    tangentNormal.xy *= normalScale;
    tangentNormal = normalize(tangentNormal);
    vec3 worldNormal = normalize(TBN * tangentNormal);

    // Pack G-buffer
    outAlbedoMetallic = vec4(albedo, finalMetallic);
    outNormalRoughness = vec4(worldNormal * 0.5 + 0.5, finalRoughness);
}
