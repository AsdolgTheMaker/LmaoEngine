#pragma once
#include "core/Window.h"
#include "core/Timer.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/Swapchain.h"
#include "vulkan/CommandPool.h"
#include "vulkan/SyncObjects.h"
#include "vulkan/DescriptorManager.h"
#include "vulkan/Buffer.h"
#include "vulkan/Image.h"
#include "vulkan/ShaderModule.h"
#include "vulkan/Pipeline.h"
#include "scene/Scene.h"
#include "math/MathUtils.h"
#include <memory>
#include <vector>

namespace lmao {

class Mesh;
class Texture;
class Material;

struct GPUPointLight {
    vec4 positionAndRange;   // xyz = position, w = range
    vec4 colorAndIntensity;  // xyz = color, w = intensity
};

enum class DebugMode : uint32_t {
    Final = 0,
    Albedo = 1,
    Metallic = 2,
    Roughness = 3,
    Normals = 4,
    Depth = 5,
};

class Engine {
public:
    Engine() = default;
    ~Engine();

    bool init();
    void run();
    void shutdown();

private:
    static constexpr uint32_t MAX_SWAPCHAIN_IMAGES = 4;
    static constexpr uint32_t MAX_POINT_LIGHTS = 256;
    static constexpr VkSampleCountFlagBits MSAA_SAMPLES = VK_SAMPLE_COUNT_4_BIT;

    bool initGBufferPass();
    bool initLightingPass();
    bool initMotionPass();
    bool initTAAPass();
    bool initTonemapPass();
    bool initFXAAPass();
    void setupDemoScene();
    void recordCommands(VkCommandBuffer cmd, uint32_t imageIndex);
    void recordGBufferPass(VkCommandBuffer cmd);
    void recordLightingPass(VkCommandBuffer cmd);
    void recordMotionPass(VkCommandBuffer cmd);
    void recordTAAPass(VkCommandBuffer cmd);
    void recordTonemapPass(VkCommandBuffer cmd);
    void recordFXAAPass(VkCommandBuffer cmd, uint32_t imageIndex);
    void drawFrame();
    void handleResize();
    void createGBufferImages();
    void createMSAAImages();
    void createHDRImage();
    void createVelocityImage();
    void createTAAImages();
    void createLDRImage();
    void updateLightingDescriptors();
    void updateAADescriptors();

    Window m_window;
    Timer m_timer;
    VulkanContext m_vkCtx;
    Swapchain m_swapchain;
    CommandPool m_cmdPool;
    FrameSync m_frameSync;
    DescriptorManager m_descriptors;

    std::vector<VkCommandBuffer> m_cmdBuffers;

    // Single-sample images (resolve targets + sampled)
    Image m_depthImage;
    Image m_gbufferRT0; // RGB = albedo, A = metallic
    Image m_gbufferRT1; // RGB = world normal, A = roughness

    // MSAA images (G-buffer primary attachments)
    Image m_gbufferRT0_MS;
    Image m_gbufferRT1_MS;
    Image m_depthImage_MS;

    // HDR target (lighting output)
    Image m_hdrImage;

    // Velocity buffer (motion vectors)
    Image m_velocityImage;

    // TAA history (ping-pong)
    Image m_taaHistory[2];
    uint32_t m_taaCurrentIdx = 0;
    uint32_t m_frameCount = 0;

    // LDR intermediate (tonemap output, FXAA input)
    Image m_ldrImage;

    // Scene
    Scene m_scene;

    // G-Buffer pass
    VkPipelineLayout m_gbufferPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_gbufferPipeline = VK_NULL_HANDLE;
    ShaderModule m_gbufferVert;
    ShaderModule m_gbufferFrag;
    VkDescriptorSetLayout m_materialSetLayout = VK_NULL_HANDLE;

    // Lighting pass
    VkPipelineLayout m_lightingPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_lightingPipeline = VK_NULL_HANDLE;
    ShaderModule m_fullscreenVert;
    ShaderModule m_lightingFrag;

    // Motion vectors pass
    VkPipelineLayout m_motionPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_motionPipeline = VK_NULL_HANDLE;
    ShaderModule m_motionFrag;

    // TAA pass
    VkPipelineLayout m_taaPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_taaPipeline = VK_NULL_HANDLE;
    ShaderModule m_taaFrag;
    VkDescriptorSetLayout m_taaSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_taaSets[2]{}; // ping-pong

    // Tonemap pass
    VkPipelineLayout m_tonemapPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_tonemapPipeline = VK_NULL_HANDLE;
    ShaderModule m_tonemapFrag;
    VkDescriptorSetLayout m_tonemapSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_tonemapSets[2]{}; // ping-pong (reads alternating TAA output)

    // FXAA pass
    VkPipelineLayout m_fxaaPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_fxaaPipeline = VK_NULL_HANDLE;
    ShaderModule m_fxaaFrag;
    VkDescriptorSetLayout m_fxaaSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_fxaaSet = VK_NULL_HANDLE;

    // Samplers
    VkSampler m_nearestSampler = VK_NULL_HANDLE; // nearest, clamp-to-edge
    VkSampler m_linearSampler = VK_NULL_HANDLE;  // linear, clamp-to-edge

    // Point lights SSBO
    Buffer m_pointLightBuffers[MAX_SWAPCHAIN_IMAGES];

    // Global UBO
    Buffer m_uniformBuffers[MAX_SWAPCHAIN_IMAGES];
    VkDescriptorSetLayout m_globalSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_globalSets[MAX_SWAPCHAIN_IMAGES]{};

    struct GlobalUBO {
        mat4 view;
        mat4 proj;            // jittered
        mat4 viewProj;        // jittered
        mat4 invViewProj;     // jittered
        mat4 prevViewProj;    // unjittered previous
        vec4 cameraPos;
        float time;
        uint32_t pointLightCount;
        float jitterX;
        float jitterY;
        vec4 dirLightDir;     // xyz = direction, w unused
        vec4 dirLightColor;   // xyz = color, w = intensity
        vec4 resolution;      // xy = width/height, zw = 1/width, 1/height
    };

    // Previous frame state for TAA
    mat4 m_prevViewProj{1.0f};

    // Debug
    DebugMode m_debugMode = DebugMode::Final;

    // Asset caches
    std::vector<std::shared_ptr<Mesh>> m_meshes;
    std::vector<std::shared_ptr<Texture>> m_textures;
    std::vector<std::shared_ptr<Material>> m_materials;

    bool m_resizeNeeded = false;
};

} // namespace lmao
