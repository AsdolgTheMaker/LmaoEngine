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
#include "math/MathUtils.h"

namespace lmao {

class Engine {
public:
    Engine() = default;
    ~Engine();

    bool init();
    void run();
    void shutdown();

private:
    bool initTriangleDemo();
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

    // Per-frame command buffers
    std::vector<VkCommandBuffer> m_cmdBuffers;

    // Depth image
    Image m_depthImage;

    // Triangle demo resources
    Buffer m_vertexBuffer;
    Buffer m_indexBuffer;
    VkPipelineLayout m_trianglePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_trianglePipeline = VK_NULL_HANDLE;
    ShaderModule m_triangleVert;
    ShaderModule m_triangleFrag;

    // Uniform buffer for MVP (one per swapchain image)
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
        float _pad[3];
    };

    bool m_resizeNeeded = false;
};

} // namespace lmao
