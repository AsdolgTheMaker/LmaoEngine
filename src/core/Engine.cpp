#include "core/Engine.h"
#include "core/Input.h"
#include "core/Log.h"
#include "vulkan/VulkanUtils.h"
#include "assets/Mesh.h"
#include "assets/MeshGenerator.h"
#include "assets/Texture.h"
#include "assets/TextureLoader.h"
#include "assets/Material.h"
#include <GLFW/glfw3.h>

namespace lmao {

Engine::~Engine() { shutdown(); }

bool Engine::init() {
    WindowConfig wc{};
    wc.width = 1600;
    wc.height = 900;
    wc.title = "LmaoEngine - Forward Demo";
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

    // Depth image
    Image::CreateInfo depthCI{};
    depthCI.width = m_swapchain.extent().width;
    depthCI.height = m_swapchain.extent().height;
    depthCI.format = VK_FORMAT_D32_SFLOAT;
    depthCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthCI.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (!m_depthImage.init(m_vkCtx.allocator(), m_vkCtx.device(), depthCI)) return false;

    // Global UBO
    VkDescriptorSetLayoutBinding globalBinding{};
    globalBinding.binding = 0;
    globalBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    globalBinding.descriptorCount = 1;
    globalBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    m_globalSetLayout = m_descriptors.getOrCreateLayout(&globalBinding, 1);

    for (uint32_t i = 0; i < m_swapchain.imageCount(); i++) {
        m_uniformBuffers[i].init(m_vkCtx.allocator(), sizeof(GlobalUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        m_globalSets[i] = m_descriptors.allocate(m_globalSetLayout);
        DescriptorManager::writeBuffer(m_vkCtx.device(), m_globalSets[i], 0,
            m_uniformBuffers[i].handle(), sizeof(GlobalUBO));
    }

    if (!initForwardPass()) return false;
    setupDemoScene();

    m_timer.reset();
    LOG(Core, Info, "Engine initialized");
    return true;
}

bool Engine::initForwardPass() {
    VkDevice device = m_vkCtx.device();

    // Load shaders
    if (!m_forwardVert.loadFromFile(device, "shaders/forward/forward.vert.spv")) return false;
    if (!m_forwardFrag.loadFromFile(device, "shaders/forward/forward.frag.spv")) return false;

    // Material descriptor set layout (set 2)
    VkDescriptorSetLayoutBinding matBindings[2] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    m_materialSetLayout = m_descriptors.getOrCreateLayout(matBindings, 2);

    // Pipeline layout: set 0 = global, set 1 = empty (reserved), set 2 = material
    // We need to create a dummy set 1 layout since we skip it
    VkDescriptorSetLayout emptyLayout;
    VkDescriptorSetLayoutCreateInfo emptyCI{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    emptyCI.bindingCount = 0;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &emptyCI, nullptr, &emptyLayout));

    VkDescriptorSetLayout setLayouts[] = {m_globalSetLayout, emptyLayout, m_materialSetLayout};

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(mat4);

    VkPipelineLayoutCreateInfo layoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutCI.setLayoutCount = 3;
    layoutCI.pSetLayouts = setLayouts;
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(device, &layoutCI, nullptr, &m_forwardPipelineLayout));

    // Clean up the empty layout (pipeline layout has copied the reference)
    vkDestroyDescriptorSetLayout(device, emptyLayout, nullptr);

    // Build pipeline
    auto binding = Vertex::bindingDesc();
    auto attrs = Vertex::attributeDescs();

    m_forwardPipeline = PipelineBuilder()
        .addShaderStage(m_forwardVert.stageInfo(VK_SHADER_STAGE_VERTEX_BIT))
        .addShaderStage(m_forwardFrag.stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT))
        .setVertexInput(&binding, 1, attrs.data(), static_cast<uint32_t>(attrs.size()))
        .setColorFormats({m_swapchain.imageFormat()})
        .setDepthFormat(VK_FORMAT_D32_SFLOAT)
        .setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .setDepthTest(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL)
        .setLayout(m_forwardPipelineLayout)
        .build(device);

    LOG(Pipeline, Info, "Forward pipeline created");
    return true;
}

void Engine::setupDemoScene() {
    auto alloc = m_vkCtx.allocator();
    auto queue = m_vkCtx.graphicsQueue();

    // Camera
    float aspect = static_cast<float>(m_swapchain.extent().width) / m_swapchain.extent().height;
    m_scene.camera().setPosition({0, 3, 8});
    m_scene.camera().setYawPitch(-HALF_PI, -0.2f);
    m_scene.camera().setPerspective(60.0f, aspect, 0.1f, 1000.0f);

    // Directional light
    auto& light = m_scene.directionalLight();
    light.direction = glm::normalize(vec3(0.3f, -1.0f, 0.5f));
    light.color = vec3(1.0f, 0.95f, 0.9f);
    light.intensity = 2.0f;

    // Generate meshes
    auto cubeMesh = MeshGenerator::createCube(alloc, queue, m_cmdPool, 1.0f);
    auto sphereMesh = MeshGenerator::createSphere(alloc, queue, m_cmdPool, 1.0f, 32, 16);
    auto planeMesh = MeshGenerator::createPlane(alloc, queue, m_cmdPool, 20.0f, 20.0f, 1, 1);
    auto torusMesh = MeshGenerator::createTorus(alloc, queue, m_cmdPool, 1.0f, 0.35f, 48, 24);
    auto cylinderMesh = MeshGenerator::createCylinder(alloc, queue, m_cmdPool, 0.5f, 2.0f, 32);
    auto coneMesh = MeshGenerator::createCone(alloc, queue, m_cmdPool, 0.7f, 1.5f, 32);
    m_meshes = {cubeMesh, sphereMesh, planeMesh, torusMesh, cylinderMesh, coneMesh};

    // Create textures
    auto whiteTex = TextureLoader::createSolidColor(m_vkCtx, m_cmdPool, vec4(1, 1, 1, 1));
    auto checkerTex = TextureLoader::createCheckerboard(m_vkCtx, m_cmdPool, 512, 32);
    m_textures = {whiteTex, checkerTex};

    // Create materials
    auto makeMat = [&](std::shared_ptr<Texture> tex, const vec4& color, float metal, float rough) {
        MaterialParams p;
        p.albedoColor = color;
        p.metallic = metal;
        p.roughness = rough;
        auto mat = std::make_shared<Material>();
        mat->init(m_vkCtx, m_descriptors, m_materialSetLayout, tex, p);
        m_materials.push_back(mat);
        return mat;
    };

    auto groundMat = makeMat(checkerTex, vec4(1, 1, 1, 1), 0.0f, 0.8f);
    auto redMat = makeMat(whiteTex, vec4(0.9f, 0.2f, 0.15f, 1), 0.0f, 0.4f);
    auto blueMat = makeMat(whiteTex, vec4(0.15f, 0.3f, 0.9f, 1), 0.3f, 0.2f);
    auto greenMat = makeMat(whiteTex, vec4(0.2f, 0.8f, 0.3f, 1), 0.0f, 0.5f);
    auto goldMat = makeMat(whiteTex, vec4(1.0f, 0.85f, 0.4f, 1), 0.9f, 0.3f);
    auto silverMat = makeMat(whiteTex, vec4(0.9f, 0.9f, 0.95f, 1), 0.95f, 0.15f);

    // Ground plane
    auto& ground = m_scene.createEntity("Ground");
    ground.mesh = planeMesh;
    ground.material = groundMat;

    // Cube
    auto& cube = m_scene.createEntity("Cube");
    cube.transform.position = {-3.0f, 0.75f, 0.0f};
    cube.transform.scale = vec3(1.5f);
    cube.mesh = cubeMesh;
    cube.material = redMat;

    // Sphere
    auto& sphere = m_scene.createEntity("Sphere");
    sphere.transform.position = {0.0f, 1.0f, 0.0f};
    sphere.mesh = sphereMesh;
    sphere.material = blueMat;

    // Torus
    auto& torus = m_scene.createEntity("Torus");
    torus.transform.position = {3.0f, 1.0f, 0.0f};
    torus.mesh = torusMesh;
    torus.material = goldMat;

    // Cylinder
    auto& cyl = m_scene.createEntity("Cylinder");
    cyl.transform.position = {-1.5f, 1.0f, -3.0f};
    cyl.mesh = cylinderMesh;
    cyl.material = greenMat;

    // Cone
    auto& cone = m_scene.createEntity("Cone");
    cone.transform.position = {1.5f, 0.75f, -3.0f};
    cone.mesh = coneMesh;
    cone.material = silverMat;

    LOG(Scene, Info, "Demo scene: %zu entities, %zu meshes, %zu materials",
        m_scene.entities().size(), m_meshes.size(), m_materials.size());
}

void Engine::recordCommands(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    Image::transitionLayout(cmd, m_swapchain.image(imageIndex),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    Image::transitionLayout(cmd, m_depthImage.handle(),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    VkRenderingAttachmentInfo colorAttach{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttach.imageView = m_swapchain.imageView(imageIndex);
    colorAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.clearValue.color = {{0.02f, 0.02f, 0.04f, 1.0f}};

    VkRenderingAttachmentInfo depthAttach{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depthAttach.imageView = m_depthImage.view();
    depthAttach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttach.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttach.clearValue.depthStencil = {0.0f, 0};

    VkRenderingInfo renderInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderInfo.renderArea = {{0, 0}, m_swapchain.extent()};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttach;
    renderInfo.pDepthAttachment = &depthAttach;

    vkCmdBeginRendering(cmd, &renderInfo);

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = static_cast<float>(m_swapchain.extent().height);
    viewport.width = static_cast<float>(m_swapchain.extent().width);
    viewport.height = -static_cast<float>(m_swapchain.extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, m_swapchain.extent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_forwardPipeline);

    uint32_t frame = m_frameSync.currentFrame();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_forwardPipelineLayout, 0, 1, &m_globalSets[frame], 0, nullptr);

    for (const auto& entity : m_scene.entities()) {
        if (!entity.mesh || !entity.material) continue;

        mat4 model = entity.transform.modelMatrix();
        vkCmdPushConstants(cmd, m_forwardPipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4), &model);

        VkDescriptorSet matSet = entity.material->descriptorSet();
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_forwardPipelineLayout, 2, 1, &matSet, 0, nullptr);

        VkBuffer vb = entity.mesh->vertexBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
        vkCmdBindIndexBuffer(cmd, entity.mesh->indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, entity.mesh->indexCount(), 1, 0, 0, 0);
    }

    vkCmdEndRendering(cmd);

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

    // Update camera
    m_scene.camera().update(m_timer.dt());

    // Update UBO
    uint32_t frame = m_frameSync.currentFrame();
    GlobalUBO ubo{};
    ubo.view = m_scene.camera().viewMatrix();
    ubo.proj = m_scene.camera().projectionMatrix();
    ubo.viewProj = ubo.proj * ubo.view;
    ubo.cameraPos = vec4(m_scene.camera().position(), 1.0f);
    ubo.time = m_timer.elapsed();

    const auto& light = m_scene.directionalLight();
    ubo.dirLightDir = vec4(light.direction, 0.0f);
    ubo.dirLightColor = vec4(light.color, light.intensity);

    m_uniformBuffers[frame].upload(&ubo, sizeof(ubo));

    VkCommandBuffer cmd = m_cmdBuffers[frame];
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    recordCommands(cmd, imageIndex);

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

    float aspect = static_cast<float>(m_swapchain.extent().width) / m_swapchain.extent().height;
    m_scene.camera().setAspect(aspect);

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

    // Clear scene entities (releases shared_ptrs)
    m_scene.entities().clear();

    // Release assets
    m_materials.clear();
    m_textures.clear();
    m_meshes.clear();

    if (m_forwardPipeline) vkDestroyPipeline(device, m_forwardPipeline, nullptr);
    if (m_forwardPipelineLayout) vkDestroyPipelineLayout(device, m_forwardPipelineLayout, nullptr);
    m_forwardPipeline = VK_NULL_HANDLE;
    m_forwardPipelineLayout = VK_NULL_HANDLE;

    m_forwardVert.shutdown();
    m_forwardFrag.shutdown();

    for (auto& ub : m_uniformBuffers) ub.shutdown();

    m_depthImage.shutdown();
    m_descriptors.shutdown();
    m_frameSync.shutdown();
    m_cmdPool.shutdown();
    m_swapchain.shutdown(device);
    m_vkCtx.shutdown();
    m_window.shutdown();
}

} // namespace lmao
