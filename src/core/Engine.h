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

class Engine {
public:
    Engine() = default;
    ~Engine();

    bool init();
    void run();
    void shutdown();

private:
    bool initForwardPass();
    void setupDemoScene();
    void recordCommands(VkCommandBuffer cmd, uint32_t imageIndex);
    void drawFrame();
    void handleResize();

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

    // Forward rendering
    VkPipelineLayout m_forwardPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_forwardPipeline = VK_NULL_HANDLE;
    ShaderModule m_forwardVert;
    ShaderModule m_forwardFrag;
    VkDescriptorSetLayout m_materialSetLayout = VK_NULL_HANDLE;

    // Global UBO
    static constexpr uint32_t MAX_SWAPCHAIN_IMAGES = 4;
    Buffer m_uniformBuffers[MAX_SWAPCHAIN_IMAGES];
    VkDescriptorSetLayout m_globalSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_globalSets[MAX_SWAPCHAIN_IMAGES]{};

    struct GlobalUBO {
        mat4 view;
        mat4 proj;
        mat4 viewProj;
        vec4 cameraPos;
        float time;
        float _pad0[3];
        vec4 dirLightDir;   // xyz = direction, w unused
        vec4 dirLightColor; // xyz = color, w = intensity
    };

    // Asset caches
    std::vector<std::shared_ptr<Mesh>> m_meshes;
    std::vector<std::shared_ptr<Texture>> m_textures;
    std::vector<std::shared_ptr<Material>> m_materials;

    bool m_resizeNeeded = false;
};

} // namespace lmao
