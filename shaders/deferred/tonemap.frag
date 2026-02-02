#version 460

layout(set = 0, binding = 0) uniform sampler2D hdrImage;
layout(set = 0, binding = 1) uniform sampler2D bloomTexture;

layout(push_constant) uniform TonemapPC {
    uint debugMode;
    float bloomIntensity;
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

// Contrast Adaptive Sharpening (AMD CAS, simplified)
// Sharpens based on local contrast — strong sharpening in low-contrast areas,
// gentle in high-contrast areas to avoid ringing
vec3 cas(vec2 uv) {
    vec2 texelSize = 1.0 / vec2(textureSize(hdrImage, 0));

    vec3 c = texture(hdrImage, uv).rgb;
    vec3 n = texture(hdrImage, uv + vec2(0, -texelSize.y)).rgb;
    vec3 s = texture(hdrImage, uv + vec2(0,  texelSize.y)).rgb;
    vec3 e = texture(hdrImage, uv + vec2( texelSize.x, 0)).rgb;
    vec3 w = texture(hdrImage, uv + vec2(-texelSize.x, 0)).rgb;

    // Find min/max of the cross neighborhood
    vec3 minNeighbor = min(c, min(min(n, s), min(e, w)));
    vec3 maxNeighbor = max(c, max(max(n, s), max(e, w)));

    // Adaptive sharpening weight: stronger where contrast is low
    // CAS formula: weight = sqrt(min / max) mapped to sharpening range
    vec3 recipMaxNeighbor = 1.0 / (maxNeighbor + vec3(0.0001));
    vec3 ampFactor = sqrt(clamp(minNeighbor * recipMaxNeighbor, 0.0, 1.0));

    // Sharpening strength (0.0 = none, 1.0 = maximum)
    float sharpness = 0.5;
    // Map to filter weight: peak at -0.125 * sharpness per tap
    vec3 filterWeight = ampFactor * (-0.125 * sharpness);

    // Apply sharpening: center + weight * (neighbors - 4 * center)
    vec3 sharpened = c + filterWeight * (n + s + e + w - 4.0 * c);

    return max(sharpened, vec3(0.0));
}

void main() {
    vec3 result;
    if (debugMode == 0u) {
        // Final: sharpen, add bloom, then tone map
        vec3 hdr = cas(fragUV);
        vec3 bloom = texture(bloomTexture, fragUV).rgb;
        hdr += bloom * bloomIntensity;
        result = acesTonemap(hdr);
    } else {
        // Debug modes: pass through (already LDR from lighting pass)
        result = texture(hdrImage, fragUV).rgb;
    }

    // No manual gamma — SRGB swapchain handles it
    outColor = vec4(result, 1.0);
}
