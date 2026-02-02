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

void main() {
    vec3 current = texture(currentHDR, fragUV).rgb;

    if (firstFrame == 1u) {
        outColor = vec4(current, 1.0);
        return;
    }

    // Read velocity and compute history UV
    vec2 velocity = texture(velocityTex, fragUV).rg;
    vec2 historyUV = fragUV + velocity;

    vec3 history = texture(historyTex, historyUV).rgb;

    // Compute velocity in pixels for adaptive decisions
    vec2 screenSize = vec2(textureSize(currentHDR, 0));
    float velocityPixels = length(velocity * screenSize);

    // Neighborhood clamp: only for moving pixels to prevent ghosting.
    // For static/near-static pixels, skip clamp so TAA can converge properly.
    // (Clamping fights the per-frame jitter variation and causes oscillation.)
    if (velocityPixels > 0.5) {
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

        vec3 aabbMin = m1 - sigma;
        vec3 aabbMax = m1 + sigma;

        vec3 historyYCoCg = RGB_to_YCoCg(history);
        historyYCoCg = clipAABB(aabbMin, aabbMax, historyYCoCg);
        history = YCoCg_to_RGB(historyYCoCg);
    }

    // Adaptive blend factor: high for static, lower for motion
    float blendFactor = mix(0.95, 0.0, clamp(velocityPixels * 0.5, 0.0, 1.0));

    // Reject history when reprojection goes off-screen
    if (any(lessThan(historyUV, vec2(0.0))) || any(greaterThan(historyUV, vec2(1.0)))) {
        blendFactor = 0.0;
    }

    outColor = vec4(mix(current, history, blendFactor), 1.0);
}
