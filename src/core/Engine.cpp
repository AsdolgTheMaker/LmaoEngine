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
#include <algorithm>
#include <cmath>
#include <cstring>

namespace lmao {

namespace {
std::shared_ptr<Texture> createBrickNormalMap(VulkanContext& ctx, CommandPool& cmdPool) {
    constexpr uint32_t size = 256;
    std::vector<uint8_t> pixels(size * size * 4);

    constexpr uint32_t brickW = 64;
    constexpr uint32_t brickH = 32;
    constexpr uint32_t mortarW = 4;

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint32_t row = y / brickH;
            uint32_t offsetX = (row % 2 == 0) ? 0 : brickW / 2;
            uint32_t bx = (x + offsetX) % brickW;
            uint32_t by = y % brickH;

            float nx = 0.0f, ny = 0.0f, nz = 1.0f;

            bool isMortarX = bx < mortarW;
            bool isMortarY = by < mortarW;

            if (isMortarX || isMortarY) {
                if (isMortarX) {
                    nx = (bx < mortarW / 2) ? -0.5f : 0.5f;
                }
                if (isMortarY) {
                    ny = (by < mortarW / 2) ? -0.5f : 0.5f;
                }
                nz = 0.7f;
                float len = std::sqrt(nx*nx + ny*ny + nz*nz);
                nx /= len; ny /= len; nz /= len;
            }

            uint32_t idx = (y * size + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>((nx * 0.5f + 0.5f) * 255.0f);
            pixels[idx + 1] = static_cast<uint8_t>((ny * 0.5f + 0.5f) * 255.0f);
            pixels[idx + 2] = static_cast<uint8_t>((nz * 0.5f + 0.5f) * 255.0f);
            pixels[idx + 3] = 255;
        }
    }

    VkDeviceSize imageSize = size * size * 4;

    Buffer staging;
    staging.init(ctx.allocator(), imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    staging.upload(pixels.data(), imageSize);

    Image::CreateInfo imgCI{};
    imgCI.width = size;
    imgCI.height = size;
    imgCI.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    Image image;
    image.init(ctx.allocator(), ctx.device(), imgCI);

    cmdPool.submitImmediate(ctx.graphicsQueue(), [&](VkCommandBuffer cmd) {
        Image::transitionLayout(cmd, image.handle(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {size, size, 1};
        vkCmdCopyBufferToImage(cmd, staging.handle(), image.handle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        Image::transitionLayout(cmd, image.handle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });
    staging.shutdown();

    auto tex = std::make_shared<Texture>();
    tex->initFromImage(ctx.device(), std::move(image), true, 1.0f);
    return tex;
}
} // anonymous namespace

Engine::~Engine() { shutdown(); }

bool Engine::init() {
    WindowConfig wc{};
    wc.width = 1600;
    wc.height = 900;
    wc.title = "LmaoEngine - Deferred PBR";
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

    // Depth image (used by G-buffer pass, also sampled in lighting pass)
    Image::CreateInfo depthCI{};
    depthCI.width = m_swapchain.extent().width;
    depthCI.height = m_swapchain.extent().height;
    depthCI.format = VK_FORMAT_D32_SFLOAT;
    depthCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depthCI.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (!m_depthImage.init(m_vkCtx.allocator(), m_vkCtx.device(), depthCI)) return false;

    // G-Buffer images
    createGBufferImages();

    // HDR image
    createHDRImage();

    // G-Buffer sampler (nearest, clamp-to-edge)
    VkSamplerCreateInfo sampCI{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampCI.magFilter = VK_FILTER_NEAREST;
    sampCI.minFilter = VK_FILTER_NEAREST;
    sampCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(m_vkCtx.device(), &sampCI, nullptr, &m_gbufferSampler));

    // Global UBO + point light SSBO descriptor set layout
    // For the G-buffer pass, we only need binding 0 (UBO) and binding 1 (SSBO)
    // For the lighting pass, we also need bindings 2-4 (G-buffer samplers)
    // We use a single layout with all 5 bindings for simplicity
    VkDescriptorSetLayoutBinding globalBindings[5] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    m_globalSetLayout = m_descriptors.getOrCreateLayout(globalBindings, 5);

    for (uint32_t i = 0; i < m_swapchain.imageCount(); i++) {
        m_uniformBuffers[i].init(m_vkCtx.allocator(), sizeof(GlobalUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

        m_pointLightBuffers[i].init(m_vkCtx.allocator(),
            sizeof(GPUPointLight) * MAX_POINT_LIGHTS,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

        m_globalSets[i] = m_descriptors.allocate(m_globalSetLayout);

        DescriptorManager::writeBuffer(m_vkCtx.device(), m_globalSets[i], 0,
            m_uniformBuffers[i].handle(), sizeof(GlobalUBO));
        DescriptorManager::writeBuffer(m_vkCtx.device(), m_globalSets[i], 1,
            m_pointLightBuffers[i].handle(), sizeof(GPUPointLight) * MAX_POINT_LIGHTS,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

    // Write G-buffer samplers to global descriptor sets
    updateLightingDescriptors();

    if (!initGBufferPass()) return false;
    if (!initLightingPass()) return false;
    if (!initTonemapPass()) return false;

    setupDemoScene();

    m_timer.reset();
    LOG(Core, Info, "Engine initialized (deferred PBR)");
    return true;
}

void Engine::createGBufferImages() {
    uint32_t w = m_swapchain.extent().width;
    uint32_t h = m_swapchain.extent().height;

    Image::CreateInfo rt0CI{};
    rt0CI.width = w;
    rt0CI.height = h;
    rt0CI.format = VK_FORMAT_R8G8B8A8_UNORM;
    rt0CI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    rt0CI.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    m_gbufferRT0.init(m_vkCtx.allocator(), m_vkCtx.device(), rt0CI);

    Image::CreateInfo rt1CI{};
    rt1CI.width = w;
    rt1CI.height = h;
    rt1CI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    rt1CI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    rt1CI.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    m_gbufferRT1.init(m_vkCtx.allocator(), m_vkCtx.device(), rt1CI);
}

void Engine::createHDRImage() {
    Image::CreateInfo hdrCI{};
    hdrCI.width = m_swapchain.extent().width;
    hdrCI.height = m_swapchain.extent().height;
    hdrCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    hdrCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    hdrCI.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    m_hdrImage.init(m_vkCtx.allocator(), m_vkCtx.device(), hdrCI);
}

void Engine::updateLightingDescriptors() {
    for (uint32_t i = 0; i < m_swapchain.imageCount(); i++) {
        DescriptorManager::writeImage(m_vkCtx.device(), m_globalSets[i], 2,
            m_gbufferRT0.view(), m_gbufferSampler);
        DescriptorManager::writeImage(m_vkCtx.device(), m_globalSets[i], 3,
            m_gbufferRT1.view(), m_gbufferSampler);
        DescriptorManager::writeImage(m_vkCtx.device(), m_globalSets[i], 4,
            m_depthImage.view(), m_gbufferSampler);
    }

    // Tonemap descriptor set
    if (!m_tonemapSet && m_tonemapSetLayout) {
        m_tonemapSet = m_descriptors.allocate(m_tonemapSetLayout);
    }
    if (m_tonemapSet) {
        DescriptorManager::writeImage(m_vkCtx.device(), m_tonemapSet, 0,
            m_hdrImage.view(), m_gbufferSampler);
    }
}

bool Engine::initGBufferPass() {
    VkDevice device = m_vkCtx.device();

    if (!m_gbufferVert.loadFromFile(device, "shaders/deferred/gbuffer.vert.spv")) return false;
    if (!m_gbufferFrag.loadFromFile(device, "shaders/deferred/gbuffer.frag.spv")) return false;

    // Material descriptor set layout (set 2): 4 bindings
    VkDescriptorSetLayoutBinding matBindings[4] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    m_materialSetLayout = m_descriptors.getOrCreateLayout(matBindings, 4);

    // Pipeline layout: set 0 = global, set 1 = empty (reserved), set 2 = material
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
    VK_CHECK(vkCreatePipelineLayout(device, &layoutCI, nullptr, &m_gbufferPipelineLayout));

    vkDestroyDescriptorSetLayout(device, emptyLayout, nullptr);

    // Build pipeline (2 color attachments + depth)
    auto binding = Vertex::bindingDesc();
    auto attrs = Vertex::attributeDescs();

    m_gbufferPipeline = PipelineBuilder()
        .addShaderStage(m_gbufferVert.stageInfo(VK_SHADER_STAGE_VERTEX_BIT))
        .addShaderStage(m_gbufferFrag.stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT))
        .setVertexInput(&binding, 1, attrs.data(), static_cast<uint32_t>(attrs.size()))
        .setColorFormats({VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT})
        .setColorBlendAttachment(2, false)
        .setDepthFormat(VK_FORMAT_D32_SFLOAT)
        .setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .setDepthTest(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL)
        .setLayout(m_gbufferPipelineLayout)
        .build(device);

    LOG(Pipeline, Info, "G-Buffer pipeline created");
    return true;
}

bool Engine::initLightingPass() {
    VkDevice device = m_vkCtx.device();

    if (!m_fullscreenVert.loadFromFile(device, "shaders/deferred/fullscreen.vert.spv")) return false;
    if (!m_lightingFrag.loadFromFile(device, "shaders/deferred/lighting.frag.spv")) return false;

    // Lighting pipeline layout: set 0 = global (with G-buffer samplers)
    VkDescriptorSetLayout setLayouts[] = {m_globalSetLayout};

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(uint32_t);

    VkPipelineLayoutCreateInfo layoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutCI.setLayoutCount = 1;
    layoutCI.pSetLayouts = setLayouts;
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(device, &layoutCI, nullptr, &m_lightingPipelineLayout));

    // Build pipeline (1 HDR color attachment, no depth)
    m_lightingPipeline = PipelineBuilder()
        .addShaderStage(m_fullscreenVert.stageInfo(VK_SHADER_STAGE_VERTEX_BIT))
        .addShaderStage(m_lightingFrag.stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT))
        .setColorFormats({VK_FORMAT_R16G16B16A16_SFLOAT})
        .setDepthTest(false, false)
        .setCullMode(VK_CULL_MODE_NONE)
        .setLayout(m_lightingPipelineLayout)
        .build(device);

    LOG(Pipeline, Info, "Lighting pipeline created");
    return true;
}

bool Engine::initTonemapPass() {
    VkDevice device = m_vkCtx.device();

    if (!m_tonemapFrag.loadFromFile(device, "shaders/deferred/tonemap.frag.spv")) return false;

    // Tonemap descriptor set layout: binding 0 = HDR sampler
    VkDescriptorSetLayoutBinding tonemapBinding{};
    tonemapBinding.binding = 0;
    tonemapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    tonemapBinding.descriptorCount = 1;
    tonemapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    m_tonemapSetLayout = m_descriptors.getOrCreateLayout(&tonemapBinding, 1);

    // Allocate and write tonemap descriptor set
    m_tonemapSet = m_descriptors.allocate(m_tonemapSetLayout);
    DescriptorManager::writeImage(m_vkCtx.device(), m_tonemapSet, 0,
        m_hdrImage.view(), m_gbufferSampler);

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(uint32_t);

    VkPipelineLayoutCreateInfo layoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutCI.setLayoutCount = 1;
    layoutCI.pSetLayouts = &m_tonemapSetLayout;
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(device, &layoutCI, nullptr, &m_tonemapPipelineLayout));

    m_tonemapPipeline = PipelineBuilder()
        .addShaderStage(m_fullscreenVert.stageInfo(VK_SHADER_STAGE_VERTEX_BIT))
        .addShaderStage(m_tonemapFrag.stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT))
        .setColorFormats({m_swapchain.imageFormat()})
        .setDepthTest(false, false)
        .setCullMode(VK_CULL_MODE_NONE)
        .setLayout(m_tonemapPipelineLayout)
        .build(device);

    LOG(Pipeline, Info, "Tonemap pipeline created");
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

    // Point lights
    {
        auto& pl = m_scene.createPointLight();
        pl.position = vec3(3.0f, 2.5f, 2.0f);
        pl.color = vec3(1.0f, 0.3f, 0.1f);
        pl.intensity = 5.0f;
        pl.range = 12.0f;
    }
    {
        auto& pl = m_scene.createPointLight();
        pl.position = vec3(-3.0f, 2.0f, -1.0f);
        pl.color = vec3(0.1f, 0.4f, 1.0f);
        pl.intensity = 5.0f;
        pl.range = 12.0f;
    }
    {
        auto& pl = m_scene.createPointLight();
        pl.position = vec3(0.0f, 3.0f, -3.0f);
        pl.color = vec3(0.2f, 1.0f, 0.3f);
        pl.intensity = 4.0f;
        pl.range = 10.0f;
    }
    {
        auto& pl = m_scene.createPointLight();
        pl.position = vec3(-1.0f, 1.5f, 3.0f);
        pl.color = vec3(1.0f, 0.8f, 0.2f);
        pl.intensity = 3.0f;
        pl.range = 8.0f;
    }

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

    // Default flat normal map (128, 128, 255) = (0.5, 0.5, 1.0) in tangent space -> pointing up
    auto flatNormalTex = TextureLoader::createSolidColor(m_vkCtx, m_cmdPool,
        vec4(128.0f/255.0f, 128.0f/255.0f, 1.0f, 1.0f), false);

    // Default metallic-roughness map (white = full values from material params)
    auto defaultMRTex = TextureLoader::createSolidColor(m_vkCtx, m_cmdPool,
        vec4(1, 1, 1, 1), false);

    // Procedural brick normal map
    auto brickNormalTex = createBrickNormalMap(m_vkCtx, m_cmdPool);

    // Procedural metallic-roughness maps
    auto roughPlasticMR = TextureLoader::createSolidColor(m_vkCtx, m_cmdPool,
        vec4(0, 1, 0, 1), false); // G=1.0 roughness, B=0.0 metallic
    auto polishedMetalMR = TextureLoader::createSolidColor(m_vkCtx, m_cmdPool,
        vec4(0, 0.15f, 1, 1), false); // G=0.15 roughness, B=1.0 metallic
    auto brushedMetalMR = TextureLoader::createSolidColor(m_vkCtx, m_cmdPool,
        vec4(0, 0.4f, 1, 1), false); // G=0.4 roughness, B=1.0 metallic

    m_textures = {whiteTex, checkerTex, flatNormalTex, defaultMRTex,
                  brickNormalTex, roughPlasticMR, polishedMetalMR, brushedMetalMR};

    // Create materials with normal + metallic-roughness maps
    auto makeMat = [&](std::shared_ptr<Texture> albedo,
                       std::shared_ptr<Texture> normal,
                       std::shared_ptr<Texture> mr,
                       const vec4& color, float metal, float rough, float normScale = 1.0f) {
        MaterialParams p;
        p.albedoColor = color;
        p.metallic = metal;
        p.roughness = rough;
        p.normalScale = normScale;
        auto mat = std::make_shared<Material>();
        mat->init(m_vkCtx, m_descriptors, m_materialSetLayout, albedo, normal, mr, p);
        m_materials.push_back(mat);
        return mat;
    };

    // Ground: brick normal map, rough dielectric
    auto groundMat = makeMat(checkerTex, brickNormalTex, defaultMRTex,
        vec4(1, 1, 1, 1), 0.0f, 0.8f, 1.0f);

    // Red rough plastic cube
    auto redMat = makeMat(whiteTex, flatNormalTex, roughPlasticMR,
        vec4(0.9f, 0.15f, 0.1f, 1), 0.0f, 0.7f);

    // Blue sphere: slightly metallic, smooth
    auto blueMat = makeMat(whiteTex, flatNormalTex, defaultMRTex,
        vec4(0.15f, 0.3f, 0.9f, 1), 0.3f, 0.2f);

    // Gold torus: polished metal
    auto goldMat = makeMat(whiteTex, flatNormalTex, polishedMetalMR,
        vec4(1.0f, 0.85f, 0.4f, 1), 1.0f, 0.15f);

    // Green cylinder: diffuse
    auto greenMat = makeMat(whiteTex, flatNormalTex, defaultMRTex,
        vec4(0.2f, 0.8f, 0.3f, 1), 0.0f, 0.5f);

    // Silver cone: brushed metal
    auto silverMat = makeMat(whiteTex, flatNormalTex, brushedMetalMR,
        vec4(0.9f, 0.9f, 0.95f, 1), 1.0f, 0.4f);

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

    LOG(Scene, Info, "Demo scene: %zu entities, %zu meshes, %zu materials, %zu point lights",
        m_scene.entities().size(), m_meshes.size(), m_materials.size(),
        m_scene.pointLights().size());
}

void Engine::recordGBufferPass(VkCommandBuffer cmd) {
    // Transition G-buffer images to color attachment
    Image::transitionLayout(cmd, m_gbufferRT0.handle(),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    Image::transitionLayout(cmd, m_gbufferRT1.handle(),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    Image::transitionLayout(cmd, m_depthImage.handle(),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    VkRenderingAttachmentInfo colorAttachments[2]{};

    colorAttachments[0] = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachments[0].imageView = m_gbufferRT0.view();
    colorAttachments[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachments[0].clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

    colorAttachments[1] = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachments[1].imageView = m_gbufferRT1.view();
    colorAttachments[1].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachments[1].clearValue.color = {{0.5f, 0.5f, 1.0f, 0.0f}};

    VkRenderingAttachmentInfo depthAttach{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depthAttach.imageView = m_depthImage.view();
    depthAttach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttach.clearValue.depthStencil = {0.0f, 0};

    VkRenderingInfo renderInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderInfo.renderArea = {{0, 0}, m_swapchain.extent()};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 2;
    renderInfo.pColorAttachments = colorAttachments;
    renderInfo.pDepthAttachment = &depthAttach;

    vkCmdBeginRendering(cmd, &renderInfo);

    // Y-flipped viewport for 3D geometry
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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gbufferPipeline);

    uint32_t frame = m_frameSync.currentFrame();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_gbufferPipelineLayout, 0, 1, &m_globalSets[frame], 0, nullptr);

    for (const auto& entity : m_scene.entities()) {
        if (!entity.mesh || !entity.material) continue;

        mat4 model = entity.transform.modelMatrix();
        vkCmdPushConstants(cmd, m_gbufferPipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4), &model);

        VkDescriptorSet matSet = entity.material->descriptorSet();
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_gbufferPipelineLayout, 2, 1, &matSet, 0, nullptr);

        VkBuffer vb = entity.mesh->vertexBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
        vkCmdBindIndexBuffer(cmd, entity.mesh->indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, entity.mesh->indexCount(), 1, 0, 0, 0);
    }

    vkCmdEndRendering(cmd);

    // Transition G-buffer + depth to shader read
    Image::transitionLayout(cmd, m_gbufferRT0.handle(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Image::transitionLayout(cmd, m_gbufferRT1.handle(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Image::transitionLayout(cmd, m_depthImage.handle(),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Engine::recordLightingPass(VkCommandBuffer cmd) {
    Image::transitionLayout(cmd, m_hdrImage.handle(),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo colorAttach{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttach.imageView = m_hdrImage.view();
    colorAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingInfo renderInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderInfo.renderArea = {{0, 0}, m_swapchain.extent()};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttach;

    vkCmdBeginRendering(cmd, &renderInfo);

    // Normal (non-flipped) viewport for fullscreen passes
    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(m_swapchain.extent().width);
    viewport.height = static_cast<float>(m_swapchain.extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, m_swapchain.extent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline);

    uint32_t frame = m_frameSync.currentFrame();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_lightingPipelineLayout, 0, 1, &m_globalSets[frame], 0, nullptr);

    uint32_t debugMode = static_cast<uint32_t>(m_debugMode);
    vkCmdPushConstants(cmd, m_lightingPipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &debugMode);

    vkCmdDraw(cmd, 3, 1, 0, 0); // Fullscreen triangle

    vkCmdEndRendering(cmd);

    // Transition HDR to shader read for tonemap
    Image::transitionLayout(cmd, m_hdrImage.handle(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Engine::recordTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex) {
    Image::transitionLayout(cmd, m_swapchain.image(imageIndex),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo colorAttach{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttach.imageView = m_swapchain.imageView(imageIndex);
    colorAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttach.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderInfo.renderArea = {{0, 0}, m_swapchain.extent()};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttach;

    vkCmdBeginRendering(cmd, &renderInfo);

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(m_swapchain.extent().width);
    viewport.height = static_cast<float>(m_swapchain.extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, m_swapchain.extent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_tonemapPipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_tonemapPipelineLayout, 0, 1, &m_tonemapSet, 0, nullptr);

    uint32_t debugMode = static_cast<uint32_t>(m_debugMode);
    vkCmdPushConstants(cmd, m_tonemapPipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &debugMode);

    vkCmdDraw(cmd, 3, 1, 0, 0); // Fullscreen triangle

    vkCmdEndRendering(cmd);

    Image::transitionLayout(cmd, m_swapchain.image(imageIndex),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void Engine::recordCommands(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    recordGBufferPass(cmd);
    recordLightingPass(cmd);
    recordTonemapPass(cmd, imageIndex);

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
    ubo.invViewProj = glm::inverse(ubo.viewProj);
    ubo.cameraPos = vec4(m_scene.camera().position(), 1.0f);
    ubo.time = m_timer.elapsed();
    ubo.pointLightCount = static_cast<uint32_t>(m_scene.pointLights().size());

    const auto& light = m_scene.directionalLight();
    ubo.dirLightDir = vec4(light.direction, 0.0f);
    ubo.dirLightColor = vec4(light.color, light.intensity);

    m_uniformBuffers[frame].upload(&ubo, sizeof(ubo));

    // Upload point lights
    const auto& pointLights = m_scene.pointLights();
    if (!pointLights.empty()) {
        std::vector<GPUPointLight> gpuLights(pointLights.size());
        for (size_t i = 0; i < pointLights.size(); i++) {
            gpuLights[i].positionAndRange = vec4(pointLights[i].position, pointLights[i].range);
            gpuLights[i].colorAndIntensity = vec4(pointLights[i].color, pointLights[i].intensity);
        }
        m_pointLightBuffers[frame].upload(gpuLights.data(),
            sizeof(GPUPointLight) * gpuLights.size());
    }

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
    m_gbufferRT0.shutdown();
    m_gbufferRT1.shutdown();
    m_hdrImage.shutdown();

    m_frameSync.shutdown();
    m_swapchain.recreate(m_vkCtx, m_window.width(), m_window.height());
    m_frameSync.init(m_vkCtx.device(), m_swapchain.imageCount());

    Image::CreateInfo depthCI{};
    depthCI.width = m_swapchain.extent().width;
    depthCI.height = m_swapchain.extent().height;
    depthCI.format = VK_FORMAT_D32_SFLOAT;
    depthCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depthCI.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    m_depthImage.init(m_vkCtx.allocator(), m_vkCtx.device(), depthCI);

    createGBufferImages();
    createHDRImage();
    updateLightingDescriptors();

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

        // Debug mode switching (keys 1-6)
        if (Input::keyPressed(GLFW_KEY_1)) m_debugMode = DebugMode::Final;
        if (Input::keyPressed(GLFW_KEY_2)) m_debugMode = DebugMode::Albedo;
        if (Input::keyPressed(GLFW_KEY_3)) m_debugMode = DebugMode::Metallic;
        if (Input::keyPressed(GLFW_KEY_4)) m_debugMode = DebugMode::Roughness;
        if (Input::keyPressed(GLFW_KEY_5)) m_debugMode = DebugMode::Normals;
        if (Input::keyPressed(GLFW_KEY_6)) m_debugMode = DebugMode::Depth;

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
    m_scene.pointLights().clear();

    // Release assets
    m_materials.clear();
    m_textures.clear();
    m_meshes.clear();

    // Destroy pipelines
    if (m_gbufferPipeline) vkDestroyPipeline(device, m_gbufferPipeline, nullptr);
    if (m_gbufferPipelineLayout) vkDestroyPipelineLayout(device, m_gbufferPipelineLayout, nullptr);
    if (m_lightingPipeline) vkDestroyPipeline(device, m_lightingPipeline, nullptr);
    if (m_lightingPipelineLayout) vkDestroyPipelineLayout(device, m_lightingPipelineLayout, nullptr);
    if (m_tonemapPipeline) vkDestroyPipeline(device, m_tonemapPipeline, nullptr);
    if (m_tonemapPipelineLayout) vkDestroyPipelineLayout(device, m_tonemapPipelineLayout, nullptr);

    m_gbufferVert.shutdown();
    m_gbufferFrag.shutdown();
    m_fullscreenVert.shutdown();
    m_lightingFrag.shutdown();
    m_tonemapFrag.shutdown();

    if (m_gbufferSampler) vkDestroySampler(device, m_gbufferSampler, nullptr);

    for (auto& ub : m_uniformBuffers) ub.shutdown();
    for (auto& plb : m_pointLightBuffers) plb.shutdown();

    m_gbufferRT0.shutdown();
    m_gbufferRT1.shutdown();
    m_hdrImage.shutdown();
    m_depthImage.shutdown();

    m_descriptors.shutdown();
    m_frameSync.shutdown();
    m_cmdPool.shutdown();
    m_swapchain.shutdown(device);
    m_vkCtx.shutdown();
    m_window.shutdown();
}

} // namespace lmao
