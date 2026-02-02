#version 460

layout(set = 0, binding = 0) uniform sampler2D srcTexture;

layout(push_constant) uniform BloomPC {
    vec2 srcTexelSize;
    int unused;
};

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
    // 3x3 tent filter upsample
    vec2 ts = srcTexelSize;

    vec3 color = vec3(0.0);
    color += texture(srcTexture, fragUV + vec2(-ts.x, -ts.y)).rgb * 1.0;
    color += texture(srcTexture, fragUV + vec2(  0.0, -ts.y)).rgb * 2.0;
    color += texture(srcTexture, fragUV + vec2( ts.x, -ts.y)).rgb * 1.0;
    color += texture(srcTexture, fragUV + vec2(-ts.x,   0.0)).rgb * 2.0;
    color += texture(srcTexture, fragUV).rgb * 4.0;
    color += texture(srcTexture, fragUV + vec2( ts.x,   0.0)).rgb * 2.0;
    color += texture(srcTexture, fragUV + vec2(-ts.x,  ts.y)).rgb * 1.0;
    color += texture(srcTexture, fragUV + vec2(  0.0,  ts.y)).rgb * 2.0;
    color += texture(srcTexture, fragUV + vec2( ts.x,  ts.y)).rgb * 1.0;
    color /= 16.0;

    outColor = vec4(color, 1.0);
}
