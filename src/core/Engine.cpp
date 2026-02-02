#include "core/Engine.h"
#include "core/Input.h"
#include "core/Log.h"
#include "vulkan/VulkanUtils.h"
#include <GLFW/glfw3.h>
#include <cmath>

namespace lmao {

Engine::~Engine() { shutdown(); }

bool Engine::init() {
    WindowConfig wc{};
    wc.width = 1600;
    wc.height = 900;
    wc.title = "LmaoEngine - Triangle Demo";
    if (!m_window.init(wc)) return false;

    Input::init(m_window.handle());

    m_window.setResizeCallback([this](uint32_t, uint32_t) {
        m_resizeNeeded = true;
    });

    if (!m_vkCtx.init(m_window.handle())) return false;
    if (!m_swapchain.init(m_vkCtx, m_window.width(), m_window.height())) return false;
    LOG(Core, Debug, "Swapchain image count: %u", m_swapchain.imageCount());
    if (!m_cmdPool.init(m_vkCtx.device(), m_vkCtx.queueFamilies().graphics)) return false;
    if (!m_frameSync.init(m_vkCtx.device(), m_swapchain.imageCount())) return false;
    if (!m_descriptors.init(m_vkCtx.device())) return false;

    m_cmdBuffers = m_cmdPool.allocate(m_swapchain.imageCount());

    // Create depth image
    Image::CreateInfo depthCI{};
    depthCI.width = m_swapchain.extent().width;
    depthCI.height = m_swapchain.extent().height;
    depthCI.format = VK_FORMAT_D32_SFLOAT;
    depthCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthCI.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (!m_depthImage.init(m_vkCtx.allocator(), m_vkCtx.device(), depthCI)) return false;

    if (!initTriangleDemo()) return false;

    m_timer.reset();
    LOG(Core, Info, "Engine initialized");
    return true;
}

bool Engine::initTriangleDemo() {
    VkDevice device = m_vkCtx.device();

    // Triangle vertices with colors baked into position (will use push constants for model matrix later)
    struct SimpleVertex {
        vec3 pos;
        vec3 color;
    };

    // A colored cube
    SimpleVertex vertices[] = {
        // Front face (red)
        {{-0.5f, -0.5f,  0.5f}, {1, 0, 0}}, {{ 0.5f, -0.5f,  0.5f}, {1, 0, 0}},
        {{ 0.5f,  0.5f,  0.5f}, {1, 0, 0}}, {{-0.5f,  0.5f,  0.5f}, {1, 0, 0}},
        // Back face (green)
        {{ 0.5f, -0.5f, -0.5f}, {0, 1, 0}}, {{-0.5f, -0.5f, -0.5f}, {0, 1, 0}},
        {{-0.5f,  0.5f, -0.5f}, {0, 1, 0}}, {{ 0.5f,  0.5f, -0.5f}, {0, 1, 0}},
        // Top face (blue)
        {{-0.5f,  0.5f,  0.5f}, {0, 0, 1}}, {{ 0.5f,  0.5f,  0.5f}, {0, 0, 1}},
        {{ 0.5f,  0.5f, -0.5f}, {0, 0, 1}}, {{-0.5f,  0.5f, -0.5f}, {0, 0, 1}},
        // Bottom face (yellow)
        {{-0.5f, -0.5f, -0.5f}, {1, 1, 0}}, {{ 0.5f, -0.5f, -0.5f}, {1, 1, 0}},
        {{ 0.5f, -0.5f,  0.5f}, {1, 1, 0}}, {{-0.5f, -0.5f,  0.5f}, {1, 1, 0}},
        // Right face (magenta)
        {{ 0.5f, -0.5f,  0.5f}, {1, 0, 1}}, {{ 0.5f, -0.5f, -0.5f}, {1, 0, 1}},
        {{ 0.5f,  0.5f, -0.5f}, {1, 0, 1}}, {{ 0.5f,  0.5f,  0.5f}, {1, 0, 1}},
        // Left face (cyan)
        {{-0.5f, -0.5f, -0.5f}, {0, 1, 1}}, {{-0.5f, -0.5f,  0.5f}, {0, 1, 1}},
        {{-0.5f,  0.5f,  0.5f}, {0, 1, 1}}, {{-0.5f,  0.5f, -0.5f}, {0, 1, 1}},
    };

    uint32_t indices[] = {
         0,  1,  2,  2,  3,  0,  // front
         4,  5,  6,  6,  7,  4,  // back
         8,  9, 10, 10, 11,  8,  // top
        12, 13, 14, 14, 15, 12,  // bottom
        16, 17, 18, 18, 19, 16,  // right
        20, 21, 22, 22, 23, 20,  // left
    };

    // Create vertex buffer (staging upload via immediate command)
    VkDeviceSize vbSize = sizeof(vertices);
    m_vertexBuffer.init(m_vkCtx.allocator(), vbSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    Buffer staging;
    staging.init(m_vkCtx.allocator(), vbSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    staging.upload(vertices, vbSize);

    m_cmdPool.submitImmediate(m_vkCtx.graphicsQueue(), [&](VkCommandBuffer cmd) {
        VkBufferCopy copy{};
        copy.size = vbSize;
        vkCmdCopyBuffer(cmd, staging.handle(), m_vertexBuffer.handle(), 1, &copy);
    });
    staging.shutdown();

    // Index buffer
    VkDeviceSize ibSize = sizeof(indices);
    m_indexBuffer.init(m_vkCtx.allocator(), ibSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    staging.init(m_vkCtx.allocator(), ibSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    staging.upload(indices, ibSize);

    m_cmdPool.submitImmediate(m_vkCtx.graphicsQueue(), [&](VkCommandBuffer cmd) {
        VkBufferCopy copy{};
        copy.size = ibSize;
        vkCmdCopyBuffer(cmd, staging.handle(), m_indexBuffer.handle(), 1, &copy);
    });
    staging.shutdown();

    // Uniform buffers (one per frame in flight)
    for (uint32_t i = 0; i < m_swapchain.imageCount(); i++) {
        m_uniformBuffers[i].init(m_vkCtx.allocator(), sizeof(GlobalUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }

    // Descriptor set layout for global UBO
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    m_globalSetLayout = m_descriptors.getOrCreateLayout(&binding, 1);

    for (uint32_t i = 0; i < m_swapchain.imageCount(); i++) {
        m_globalSets[i] = m_descriptors.allocate(m_globalSetLayout);
        DescriptorManager::writeBuffer(device, m_globalSets[i], 0,
            m_uniformBuffers[i].handle(), sizeof(GlobalUBO));
    }

    // Push constant for model matrix
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(mat4);

    VkPipelineLayoutCreateInfo layoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutCI.setLayoutCount = 1;
    layoutCI.pSetLayouts = &m_globalSetLayout;
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(device, &layoutCI, nullptr, &m_trianglePipelineLayout));

    // Load shaders
    if (!m_triangleVert.loadFromFile(device, "shaders/debug/triangle.vert.spv")) return false;
    if (!m_triangleFrag.loadFromFile(device, "shaders/debug/triangle.frag.spv")) return false;

    // Vertex input for SimpleVertex
    VkVertexInputBindingDescription vbind{0, sizeof(SimpleVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription vattrs[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(vec3)},
    };

    m_trianglePipeline = PipelineBuilder()
        .addShaderStage(m_triangleVert.stageInfo(VK_SHADER_STAGE_VERTEX_BIT))
        .addShaderStage(m_triangleFrag.stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT))
        .setVertexInput(&vbind, 1, vattrs, 2)
        .setColorFormats({m_swapchain.imageFormat()})
        .setDepthFormat(VK_FORMAT_D32_SFLOAT)
        .setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .setDepthTest(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL)
        .setLayout(m_trianglePipelineLayout)
        .build(device);

    return true;
}

void Engine::recordCommands(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // Transition swapchain image to color attachment
    Image::transitionLayout(cmd, m_swapchain.image(imageIndex),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Transition depth to depth attachment
    Image::transitionLayout(cmd, m_depthImage.handle(),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    // Begin dynamic rendering
    VkRenderingAttachmentInfo colorAttach{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttach.imageView = m_swapchain.imageView(imageIndex);
    colorAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.clearValue.color = {{0.01f, 0.01f, 0.02f, 1.0f}};

    VkRenderingAttachmentInfo depthAttach{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depthAttach.imageView = m_depthImage.view();
    depthAttach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttach.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttach.clearValue.depthStencil = {0.0f, 0}; // reversed-Z: clear to 0

    VkRenderingInfo renderInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderInfo.renderArea = {{0, 0}, m_swapchain.extent()};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttach;
    renderInfo.pDepthAttachment = &depthAttach;

    vkCmdBeginRendering(cmd, &renderInfo);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = static_cast<float>(m_swapchain.extent().height);
    viewport.width = static_cast<float>(m_swapchain.extent().width);
    viewport.height = -static_cast<float>(m_swapchain.extent().height); // flip Y for GLM compatibility
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, m_swapchain.extent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_trianglePipeline);

    // Bind global descriptor set
    uint32_t frame = m_frameSync.currentFrame();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_trianglePipelineLayout, 0, 1, &m_globalSets[frame], 0, nullptr);

    // Push model matrix (rotating cube)
    float angle = m_timer.elapsed() * 0.5f;
    mat4 model = glm::rotate(mat4(1.0f), angle, vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, angle * 0.3f, vec3(1.0f, 0.0f, 0.0f));
    vkCmdPushConstants(cmd, m_trianglePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4), &model);

    // Draw
    VkBuffer vb = m_vertexBuffer.handle();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer.handle(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

    vkCmdEndRendering(cmd);

    // Transition swapchain image to present
    Image::transitionLayout(cmd, m_swapchain.image(imageIndex),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));
}

void Engine::drawFrame() {
    VkDevice device = m_vkCtx.device();
    m_frameSync.waitForFence(device);

    uint32_t imageIndex = m_swapchain.acquireNextImage(device, m_frameSync.imageAvailableSemaphore());
    if (imageIndex == UINT32_MAX) {
        handleResize();
        return;
    }

    m_frameSync.resetFence(device);

    // Update UBO
    uint32_t frame = m_frameSync.currentFrame();
    float aspect = static_cast<float>(m_swapchain.extent().width) / static_cast<float>(m_swapchain.extent().height);

    GlobalUBO ubo{};
    vec3 eye(0.0f, 0.5f, 3.0f);
    ubo.view = glm::lookAt(eye, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
    // Reversed-Z: swap near/far in projection
    ubo.proj = glm::perspective(glm::radians(60.0f), aspect, 1000.0f, 0.1f);
    ubo.viewProj = ubo.proj * ubo.view;
    ubo.cameraPos = vec4(eye, 1.0f);
    ubo.time = m_timer.elapsed();
    m_uniformBuffers[frame].upload(&ubo, sizeof(ubo));

    // Record
    VkCommandBuffer cmd = m_cmdBuffers[frame];
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    recordCommands(cmd, imageIndex);

    // Submit
    VkSemaphore waitSems[] = {m_frameSync.imageAvailableSemaphore()};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSems[] = {m_frameSync.renderFinishedSemaphore()};

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSems;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSems;

    VK_CHECK(vkQueueSubmit(m_vkCtx.graphicsQueue(), 1, &submitInfo, m_frameSync.inFlightFence()));

    VkResult presentResult = m_swapchain.present(m_vkCtx.presentQueue(), imageIndex, m_frameSync.renderFinishedSemaphore());
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || m_resizeNeeded) {
        handleResize();
    }

    m_frameSync.advance();
}

void Engine::handleResize() {
    m_resizeNeeded = false;
    LOG(Swapchain, Info, "Resize triggered: %ux%u", m_window.width(), m_window.height());
    m_vkCtx.waitIdle();

    m_depthImage.shutdown();
    m_frameSync.shutdown();
    m_swapchain.recreate(m_vkCtx, m_window.width(), m_window.height());
    m_frameSync.init(m_vkCtx.device(), m_swapchain.imageCount());

    Image::CreateInfo depthCI{};
    depthCI.width = m_swapchain.extent().width;
    depthCI.height = m_swapchain.extent().height;
    depthCI.format = VK_FORMAT_D32_SFLOAT;
    depthCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthCI.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    m_depthImage.init(m_vkCtx.allocator(), m_vkCtx.device(), depthCI);

    m_window.clearResizedFlag();
}

void Engine::run() {
    while (!m_window.shouldClose()) {
        m_window.pollEvents();
        m_timer.tick();

        if (Input::keyPressed(GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(m_window.handle(), GLFW_TRUE);

        drawFrame();
        Input::endFrame();
    }

    m_vkCtx.waitIdle();
}

void Engine::shutdown() {
    VkDevice device = m_vkCtx.device();
    if (!device) return;
    LOG(Core, Info, "Engine shutting down");

    m_vkCtx.waitIdle();

    if (m_trianglePipeline) vkDestroyPipeline(device, m_trianglePipeline, nullptr);
    if (m_trianglePipelineLayout) vkDestroyPipelineLayout(device, m_trianglePipelineLayout, nullptr);
    m_trianglePipeline = VK_NULL_HANDLE;
    m_trianglePipelineLayout = VK_NULL_HANDLE;

    m_triangleVert.shutdown();
    m_triangleFrag.shutdown();

    for (auto& ub : m_uniformBuffers) ub.shutdown();
    m_vertexBuffer.shutdown();
    m_indexBuffer.shutdown();

    m_depthImage.shutdown();
    m_descriptors.shutdown();
    m_frameSync.shutdown();
    m_cmdPool.shutdown();
    m_swapchain.shutdown(device);
    m_vkCtx.shutdown();
    m_window.shutdown();
}

} // namespace lmao
