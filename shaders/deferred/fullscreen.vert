#version 460

layout(location = 0) out vec2 fragUV;

void main() {
    // Generate fullscreen triangle from gl_VertexIndex (0, 1, 2)
    // Produces a triangle that covers the entire screen
    fragUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragUV * 2.0 - 1.0, 0.0, 1.0);
    // Flip Y for Vulkan coordinate system
    fragUV.y = 1.0 - fragUV.y;
}
