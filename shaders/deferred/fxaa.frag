#version 460

layout(set = 0, binding = 0) uniform sampler2D inputTex;

layout(push_constant) uniform FXAAPC {
    vec2 inverseScreenSize;
};

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

// FXAA quality settings
const float EDGE_THRESHOLD_MIN = 0.0625;
const float EDGE_THRESHOLD_MAX = 0.166;
const float SUBPIXEL_QUALITY = 0.5;
const int SEARCH_STEPS = 12;

float luminance(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec3 colorCenter = texture(inputTex, fragUV).rgb;
    float lumaCenter = luminance(colorCenter);

    // Sample 4 neighbors
    float lumaN = luminance(textureOffset(inputTex, fragUV, ivec2( 0,  1)).rgb);
    float lumaS = luminance(textureOffset(inputTex, fragUV, ivec2( 0, -1)).rgb);
    float lumaE = luminance(textureOffset(inputTex, fragUV, ivec2( 1,  0)).rgb);
    float lumaW = luminance(textureOffset(inputTex, fragUV, ivec2(-1,  0)).rgb);

    // Find the maximum and minimum luma around current
    float lumaMin = min(lumaCenter, min(min(lumaN, lumaS), min(lumaE, lumaW)));
    float lumaMax = max(lumaCenter, max(max(lumaN, lumaS), max(lumaE, lumaW)));

    float lumaRange = lumaMax - lumaMin;

    // Early exit for low contrast
    if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX)) {
        outColor = vec4(colorCenter, 1.0);
        return;
    }

    // Sample diagonal neighbors
    float lumaNW = luminance(textureOffset(inputTex, fragUV, ivec2(-1,  1)).rgb);
    float lumaNE = luminance(textureOffset(inputTex, fragUV, ivec2( 1,  1)).rgb);
    float lumaSW = luminance(textureOffset(inputTex, fragUV, ivec2(-1, -1)).rgb);
    float lumaSE = luminance(textureOffset(inputTex, fragUV, ivec2( 1, -1)).rgb);

    float lumaNS = lumaN + lumaS;
    float lumaWE = lumaW + lumaE;

    // Determine edge direction
    float edgeHorz = abs(-2.0 * lumaW + lumaNW + lumaSW) +
                     abs(-2.0 * lumaCenter + lumaN + lumaS) * 2.0 +
                     abs(-2.0 * lumaE + lumaNE + lumaSE);
    float edgeVert = abs(-2.0 * lumaN + lumaNW + lumaNE) +
                     abs(-2.0 * lumaCenter + lumaW + lumaE) * 2.0 +
                     abs(-2.0 * lumaS + lumaSW + lumaSE);

    bool isHorizontal = edgeHorz >= edgeVert;

    // Select edge direction step
    float stepLength = isHorizontal ? inverseScreenSize.y : inverseScreenSize.x;

    float luma1 = isHorizontal ? lumaS : lumaW;
    float luma2 = isHorizontal ? lumaN : lumaE;

    float gradient1 = luma1 - lumaCenter;
    float gradient2 = luma2 - lumaCenter;

    bool is1Steepest = abs(gradient1) >= abs(gradient2);

    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

    if (is1Steepest) {
        stepLength = -stepLength;
    }

    // Shift UV to edge center
    vec2 currentUV = fragUV;
    float lumaLocalAvg;
    if (isHorizontal) {
        currentUV.y += stepLength * 0.5;
        lumaLocalAvg = lumaCenter + (is1Steepest ? gradient1 : gradient2) * 0.5;
    } else {
        currentUV.x += stepLength * 0.5;
        lumaLocalAvg = lumaCenter + (is1Steepest ? gradient1 : gradient2) * 0.5;
    }

    // Search along edge in both directions
    vec2 offset = isHorizontal ? vec2(inverseScreenSize.x, 0.0) : vec2(0.0, inverseScreenSize.y);

    vec2 uv1 = currentUV - offset;
    vec2 uv2 = currentUV + offset;

    float lumaEnd1 = luminance(texture(inputTex, uv1).rgb) - lumaLocalAvg;
    float lumaEnd2 = luminance(texture(inputTex, uv2).rgb) - lumaLocalAvg;

    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;

    const float QUALITY[12] = float[](1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);

    for (int i = 0; i < SEARCH_STEPS && !(reached1 && reached2); i++) {
        if (!reached1) {
            uv1 -= offset * QUALITY[i];
            lumaEnd1 = luminance(texture(inputTex, uv1).rgb) - lumaLocalAvg;
            reached1 = abs(lumaEnd1) >= gradientScaled;
        }
        if (!reached2) {
            uv2 += offset * QUALITY[i];
            lumaEnd2 = luminance(texture(inputTex, uv2).rgb) - lumaLocalAvg;
            reached2 = abs(lumaEnd2) >= gradientScaled;
        }
    }

    // Compute distances
    float dist1 = isHorizontal ? (fragUV.x - uv1.x) : (fragUV.y - uv1.y);
    float dist2 = isHorizontal ? (uv2.x - fragUV.x) : (uv2.y - fragUV.y);

    float distFinal = min(dist1, dist2);
    float edgeLength = dist1 + dist2;
    float pixelOffset = -distFinal / edgeLength + 0.5;

    // Sub-pixel anti-aliasing
    float lumaAvg = (lumaNS + lumaWE) * 0.25 + (lumaNW + lumaNE + lumaSW + lumaSE) * 0.125;
    float subPixelOffset = clamp(abs(lumaAvg - lumaCenter) / lumaRange, 0.0, 1.0);
    subPixelOffset = (-2.0 * subPixelOffset + 3.0) * subPixelOffset * subPixelOffset;
    float subPixelOffsetFinal = subPixelOffset * subPixelOffset * SUBPIXEL_QUALITY;

    // Check end-of-edge luma signs
    bool correctVariation1 = (lumaEnd1 < 0.0) != (lumaCenter - lumaLocalAvg < 0.0);
    bool correctVariation2 = (lumaEnd2 < 0.0) != (lumaCenter - lumaLocalAvg < 0.0);

    float finalOffset = max(
        correctVariation1 || correctVariation2 ? pixelOffset : 0.0,
        subPixelOffsetFinal
    );

    // Apply offset
    vec2 finalUV = fragUV;
    if (isHorizontal) {
        finalUV.y += finalOffset * stepLength;
    } else {
        finalUV.x += finalOffset * stepLength;
    }

    outColor = vec4(texture(inputTex, finalUV).rgb, 1.0);
}
