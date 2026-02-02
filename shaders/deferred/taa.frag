#version 460

layout(set = 0, binding = 0) uniform sampler2D currentHDR;
layout(set = 0, binding = 1) uniform sampler2D velocityTex;
layout(set = 0, binding = 2) uniform sampler2D historyTex;

layout(push_constant) uniform TAAPC {
    uint firstFrame;
};

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

vec3 RGB_to_YCoCg(vec3 rgb) {
    return vec3(
         0.25 * rgb.r + 0.5 * rgb.g + 0.25 * rgb.b,
         0.5  * rgb.r                - 0.5  * rgb.b,
        -0.25 * rgb.r + 0.5 * rgb.g - 0.25 * rgb.b
    );
}

vec3 YCoCg_to_RGB(vec3 ycocg) {
    return vec3(
        ycocg.x + ycocg.y - ycocg.z,
        ycocg.x            + ycocg.z,
        ycocg.x - ycocg.y - ycocg.z
    );
}

// Clip toward AABB center (softer than hard clamp)
vec3 clipAABB(vec3 aabbMin, vec3 aabbMax, vec3 p) {
    vec3 center = 0.5 * (aabbMin + aabbMax);
    vec3 halfSize = 0.5 * (aabbMax - aabbMin) + vec3(0.0001);
    vec3 offset = p - center;
    vec3 unit = offset / halfSize;
    vec3 absUnit = abs(unit);
    float maxComp = max(absUnit.x, max(absUnit.y, absUnit.z));
    if (maxComp > 1.0) {
        return center + offset / maxComp;
    }
    return p;
}

// Catmull-Rom bicubic sampling (5 bilinear taps)
// Produces sharper history than basic bilinear, reducing TAA blur
vec3 sampleHistoryCatmullRom(vec2 uv) {
    vec2 texSize = vec2(textureSize(historyTex, 0));
    vec2 samplePos = uv * texSize;
    vec2 tc = floor(samplePos - 0.5) + 0.5;
    vec2 f = samplePos - tc;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    // Catmull-Rom weights
    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w3 = 0.5 * (f3 - f2);
    vec2 w2 = 1.0 - w0 - w1 - w3;

    // Collapse to 5 bilinear samples
    vec2 w12 = w1 + w2;
    vec2 tc0 = (tc - 1.0) / texSize;
    vec2 tc12 = (tc + w2 / w12) / texSize;
    vec2 tc3 = (tc + 2.0) / texSize;

    vec3 result =
        texture(historyTex, vec2(tc12.x, tc0.y)).rgb  * (w12.x * w0.y) +
        texture(historyTex, vec2(tc0.x,  tc12.y)).rgb * (w0.x  * w12.y) +
        texture(historyTex, vec2(tc12.x, tc12.y)).rgb * (w12.x * w12.y) +
        texture(historyTex, vec2(tc3.x,  tc12.y)).rgb * (w3.x  * w12.y) +
        texture(historyTex, vec2(tc12.x, tc3.y)).rgb  * (w12.x * w3.y);

    // Catmull-Rom can produce negative values; clamp
    return max(result, vec3(0.0));
}

void main() {
    vec3 current = texture(currentHDR, fragUV).rgb;

    if (firstFrame == 1u) {
        outColor = vec4(current, 1.0);
        return;
    }

    // Read velocity and compute history UV
    vec2 velocity = texture(velocityTex, fragUV).rg;
    vec2 historyUV = fragUV + velocity;

    // Reject history when reprojection goes off-screen
    if (any(lessThan(historyUV, vec2(0.0))) || any(greaterThan(historyUV, vec2(1.0)))) {
        outColor = vec4(current, 1.0);
        return;
    }

    // Sample history with Catmull-Rom for sharper results
    vec3 history = sampleHistoryCatmullRom(historyUV);

    // Compute velocity in pixels
    vec2 screenSize = vec2(textureSize(currentHDR, 0));
    float velocityPixels = length(velocity * screenSize);

    // Neighborhood variance clamp in YCoCg space
    vec2 texelSize = 1.0 / screenSize;
    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec3 s = RGB_to_YCoCg(texture(currentHDR, fragUV + vec2(x, y) * texelSize).rgb);
            m1 += s;
            m2 += s * s;
        }
    }

    m1 /= 9.0;
    m2 /= 9.0;
    vec3 sigma = sqrt(max(m2 - m1 * m1, vec3(0.0)));

    // Tighter clamp for moving pixels, looser for static (allows convergence)
    float gammaClamp = mix(1.25, 0.75, clamp(velocityPixels * 2.0, 0.0, 1.0));
    vec3 aabbMin = m1 - gammaClamp * sigma;
    vec3 aabbMax = m1 + gammaClamp * sigma;

    vec3 historyYCoCg = RGB_to_YCoCg(history);
    historyYCoCg = clipAABB(aabbMin, aabbMax, historyYCoCg);
    history = YCoCg_to_RGB(historyYCoCg);

    // Blend factor: exponential falloff for fast motion rejection
    // Static (0 px): 0.95 — heavy history for convergence
    // 0.5 px: ~0.20 — mostly current frame
    // 1+ px: ~0.05 — nearly all current frame
    float blendFactor = exp(-velocityPixels * 3.0) * 0.95;
    blendFactor = clamp(blendFactor, 0.0, 0.95);

    outColor = vec4(mix(current, history, blendFactor), 1.0);
}
