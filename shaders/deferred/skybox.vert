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
};

layout(location = 0) out vec3 fragDir;

void main() {
    // Fullscreen triangle
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec4 clipPos = vec4(uv * 2.0 - 1.0, 0.0, 1.0);

    // Compute view-space ray direction from projection matrix diagonal
    // proj[0][0] = 1/(aspect*tan(fov/2)), proj[1][1] = 1/tan(fov/2)
    // These are NOT affected by TAA jitter (jitter only modifies proj[2][0/1])
    vec2 ndc = vec2(clipPos.x, -clipPos.y); // flip Y for Vulkan
    vec3 viewDir = vec3(ndc.x / proj[0][0], ndc.y / proj[1][1], -1.0);

    // Rotate from view space to world space (transpose of mat3(view) = inverse rotation)
    fragDir = transpose(mat3(view)) * viewDir;

    gl_Position = clipPos;
}
