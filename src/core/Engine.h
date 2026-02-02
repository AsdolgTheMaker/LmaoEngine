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

    bool initGBufferPass();
    bool initLightingPass();
    bool initTonemapPass();
    void setupDemoScene();
    void recordCommands(VkCommandBuffer cmd, uint32_t imageIndex);
    void recordGBufferPass(VkCommandBuffer cmd);
    void recordLightingPass(VkCommandBuffer cmd);
    void recordTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex);
    void drawFrame();
    void handleResize();
    void createGBufferImages();
    void createHDRImage();
    void updateLightingDescriptors();

    Window m_window;
    Timer m_timer;
    VulkanContext m_vkCtx;
    Swapchain m_swapchain;
    CommandPool m_cmdPool;
    FrameSync m_frameSync;
    DescriptorManager m_descriptors;

    std::vector<VkCommandBuffer> m_cmdBuffers;
    Image m_depthImage;

    // Scene
    Scene m_scene;

    // G-Buffer images
    Image m_gbufferRT0; // RGB = albedo, A = metallic
    Image m_gbufferRT1; // RGB = world normal, A = roughness

    // HDR target
    Image m_hdrImage;

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
    VkDescriptorSetLayout m_lightingSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_lightingSets[MAX_SWAPCHAIN_IMAGES]{};

    // Tonemap pass
    VkPipelineLayout m_tonemapPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_tonemapPipeline = VK_NULL_HANDLE;
    ShaderModule m_tonemapFrag;
    VkDescriptorSetLayout m_tonemapSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_tonemapSet = VK_NULL_HANDLE;

    // G-Buffer sampler (nearest, clamp-to-edge)
    VkSampler m_gbufferSampler = VK_NULL_HANDLE;

    // Point lights SSBO
    Buffer m_pointLightBuffers[MAX_SWAPCHAIN_IMAGES];

    // Global UBO
    Buffer m_uniformBuffers[MAX_SWAPCHAIN_IMAGES];
    VkDescriptorSetLayout m_globalSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_globalSets[MAX_SWAPCHAIN_IMAGES]{};

    struct GlobalUBO {
        mat4 view;
        mat4 proj;
        mat4 viewProj;
        mat4 invViewProj;
        vec4 cameraPos;
        float time;
        uint32_t pointLightCount;
        float _pad0[2];
        vec4 dirLightDir;   // xyz = direction, w unused
        vec4 dirLightColor; // xyz = color, w = intensity
    };

    // Debug
    DebugMode m_debugMode = DebugMode::Final;

    // Asset caches
    std::vector<std::shared_ptr<Mesh>> m_meshes;
    std::vector<std::shared_ptr<Texture>> m_textures;
    std::vector<std::shared_ptr<Material>> m_materials;

    bool m_resizeNeeded = false;
};

} // namespace lmao
