#include "assets/Material.h"
#include "assets/Texture.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/DescriptorManager.h"
#include "core/Log.h"

namespace lmao {

bool Material::init(VulkanContext& ctx, DescriptorManager& descMgr,
                    VkDescriptorSetLayout layout,
                    std::shared_ptr<Texture> albedoTex,
                    std::shared_ptr<Texture> normalTex,
                    std::shared_ptr<Texture> metalRoughTex,
                    const MaterialParams& params) {
    m_albedoTex = std::move(albedoTex);
    m_normalTex = std::move(normalTex);
    m_metalRoughTex = std::move(metalRoughTex);
    m_params = params;

    // Create params UBO
    m_paramsBuffer.init(ctx.allocator(), sizeof(MaterialParams),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_paramsBuffer.upload(&m_params, sizeof(MaterialParams));

    // Allocate descriptor set
    m_descriptorSet = descMgr.allocate(layout);

    // Write albedo texture (binding 0)
    DescriptorManager::writeImage(ctx.device(), m_descriptorSet, 0,
        m_albedoTex->imageView(), m_albedoTex->sampler());

    // Write normal map (binding 1)
    DescriptorManager::writeImage(ctx.device(), m_descriptorSet, 1,
        m_normalTex->imageView(), m_normalTex->sampler());

    // Write metallic-roughness map (binding 2)
    DescriptorManager::writeImage(ctx.device(), m_descriptorSet, 2,
        m_metalRoughTex->imageView(), m_metalRoughTex->sampler());

    // Write params UBO (binding 3)
    DescriptorManager::writeBuffer(ctx.device(), m_descriptorSet, 3,
        m_paramsBuffer.handle(), sizeof(MaterialParams));

    LOG(Assets, Debug, "Material created: albedo=(%.2f,%.2f,%.2f) metallic=%.2f roughness=%.2f normalScale=%.2f",
        params.albedoColor.r, params.albedoColor.g, params.albedoColor.b,
        params.metallic, params.roughness, params.normalScale);
    return true;
}

void Material::shutdown() {
    m_paramsBuffer.shutdown();
    m_albedoTex.reset();
    m_normalTex.reset();
    m_metalRoughTex.reset();
    m_descriptorSet = VK_NULL_HANDLE;
}

} // namespace lmao
