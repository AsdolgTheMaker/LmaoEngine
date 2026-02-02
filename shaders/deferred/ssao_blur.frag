#version 460

layout(set = 0, binding = 0) uniform sampler2D ssaoRaw;
layout(set = 0, binding = 1) uniform sampler2D gDepth;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out float outAO;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoRaw, 0));
    float centerDepth = texture(gDepth, fragUV).r;

    float result = 0.0;
    float totalWeight = 0.0;

    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 sampleUV = fragUV + offset;

            float ao = texture(ssaoRaw, sampleUV).r;
            float sampleDepth = texture(gDepth, sampleUV).r;

            // Bilateral weight: attenuate based on depth difference
            float depthDiff = abs(centerDepth - sampleDepth);
            float weight = exp(-depthDiff * 1000.0);

            result += ao * weight;
            totalWeight += weight;
        }
    }

    outAO = result / totalWeight;
}
