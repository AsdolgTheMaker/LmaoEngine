#version 460

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Simple directional lighting
    vec3 normal = normalize(cross(dFdx(fragWorldPos), dFdy(fragWorldPos)));
    vec3 lightDir = normalize(vec3(1.0, 1.0, 0.5));
    float NdotL = max(dot(normal, lightDir), 0.0);

    vec3 ambient = 0.15 * fragColor;
    vec3 diffuse = NdotL * fragColor;

    outColor = vec4(ambient + diffuse, 1.0);
}
