#version 460

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
    float time;
    float _pad0[3];
    vec4 dirLightDir;   // xyz = direction
    vec4 dirLightColor; // xyz = color, w = intensity
};

layout(set = 2, binding = 0) uniform sampler2D albedoMap;
layout(set = 2, binding = 1) uniform MaterialUBO {
    vec4 albedoColor;
    float metallic;
    float roughness;
    float _matPad[2];
};

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec3 fragBitangent;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 albedo = texture(albedoMap, fragUV).rgb * albedoColor.rgb;

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-dirLightDir.xyz);
    vec3 V = normalize(cameraPos.xyz - fragWorldPos);
    vec3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    // Ambient
    vec3 ambient = 0.05 * albedo;

    // Diffuse
    float intensity = dirLightColor.w;
    vec3 diffuse = NdotL * albedo * dirLightColor.rgb * intensity;

    // Blinn-Phong specular
    float shininess = mix(256.0, 8.0, roughness);
    float spec = pow(NdotH, shininess) * NdotL;
    vec3 specular = spec * dirLightColor.rgb * intensity * mix(0.04, 1.0, metallic);

    vec3 color = ambient + diffuse + specular;

    // Simple tone mapping + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
