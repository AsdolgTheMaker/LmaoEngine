#version 460

layout(set = 0, binding = 0) uniform sampler2D hdrImage;

layout(push_constant) uniform TonemapPC {
    uint debugMode;
};

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

// ACES filmic tone mapping (Narkowicz fit)
vec3 acesTonemap(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(hdrImage, fragUV).rgb;

    vec3 result;
    if (debugMode == 0u) {
        // Final: apply ACES tone mapping
        result = acesTonemap(hdr);
    } else {
        // Debug modes: pass through (already LDR from lighting pass)
        result = hdr;
    }

    // No manual gamma — SRGB swapchain handles it
    outColor = vec4(result, 1.0);
}
