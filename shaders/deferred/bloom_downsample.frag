#version 460

layout(set = 0, binding = 0) uniform sampler2D srcTexture;

layout(push_constant) uniform BloomPC {
    vec2 srcTexelSize;
    int firstMip; // 1 = apply threshold + Karis average
};

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

float luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

// Karis average: weight by 1/(1+luma) to prevent fireflies
vec3 karisAverage(vec3 a, vec3 b, vec3 c, vec3 d) {
    float wa = 1.0 / (1.0 + luminance(a));
    float wb = 1.0 / (1.0 + luminance(b));
    float wc = 1.0 / (1.0 + luminance(c));
    float wd = 1.0 / (1.0 + luminance(d));
    return (a * wa + b * wb + c * wc + d * wd) / (wa + wb + wc + wd);
}

void main() {
    vec2 ts = srcTexelSize;

    // 13-tap downsample (CoD: Advanced Warfare, Jimenez 2014)
    vec3 a = texture(srcTexture, fragUV + vec2(-2, -2) * ts).rgb;
    vec3 b = texture(srcTexture, fragUV + vec2( 0, -2) * ts).rgb;
    vec3 c = texture(srcTexture, fragUV + vec2( 2, -2) * ts).rgb;

    vec3 d = texture(srcTexture, fragUV + vec2(-2,  0) * ts).rgb;
    vec3 e = texture(srcTexture, fragUV).rgb;
    vec3 f = texture(srcTexture, fragUV + vec2( 2,  0) * ts).rgb;

    vec3 g = texture(srcTexture, fragUV + vec2(-2,  2) * ts).rgb;
    vec3 h = texture(srcTexture, fragUV + vec2( 0,  2) * ts).rgb;
    vec3 i = texture(srcTexture, fragUV + vec2( 2,  2) * ts).rgb;

    vec3 j = texture(srcTexture, fragUV + vec2(-1, -1) * ts).rgb;
    vec3 k = texture(srcTexture, fragUV + vec2( 1, -1) * ts).rgb;
    vec3 l = texture(srcTexture, fragUV + vec2(-1,  1) * ts).rgb;
    vec3 m = texture(srcTexture, fragUV + vec2( 1,  1) * ts).rgb;

    vec3 color;
    if (firstMip == 1) {
        // Karis average on 5 groups to prevent fireflies
        vec3 g0 = karisAverage(a, b, d, e);
        vec3 g1 = karisAverage(b, c, e, f);
        vec3 g2 = karisAverage(d, e, g, h);
        vec3 g3 = karisAverage(e, f, h, i);
        vec3 g4 = karisAverage(j, k, l, m);
        color = g0 * 0.125 + g1 * 0.125 + g2 * 0.125 + g3 * 0.125 + g4 * 0.5;

        // Threshold: only keep bright areas (soft knee)
        float luma = luminance(color);
        float knee = 1.0;
        float soft = clamp(luma - knee + 0.5, 0.0, 1.0);
        soft = soft * soft * (1.0 / (2.0 * 0.5 + 0.0001));
        float contribution = max(soft, luma - knee) / max(luma, 0.0001);
        color *= contribution;
    } else {
        // Standard weighted downsample
        color = e * 0.125;
        color += (j + k + l + m) * 0.125;
        color += (a + c + g + i) * 0.03125;
        color += (b + d + f + h) * 0.0625;
    }

    outColor = vec4(color, 1.0);
}
