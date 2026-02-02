#include "assets/Texture.h"
#include "core/Log.h"
#include <utility>

namespace lmao {

Texture::~Texture() { release(); }

Texture::Texture(Texture&& o) noexcept
    : m_device(o.m_device), m_image(std::move(o.m_image)), m_sampler(o.m_sampler) {
    o.m_sampler = VK_NULL_HANDLE;
}

Texture& Texture::operator=(Texture&& o) noexcept {
    if (this != &o) {
        release();
        m_device = o.m_device;
        m_image = std::move(o.m_image);
        m_sampler = o.m_sampler;
        o.m_sampler = VK_NULL_HANDLE;
    }
    return *this;
}

bool Texture::initFromImage(VkDevice device, Image&& image, bool linearFilter, float maxAniso) {
    m_device = device;
    m_image = std::move(image);

    VkSamplerCreateInfo ci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    ci.magFilter = linearFilter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    ci.minFilter = linearFilter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    ci.mipmapMode = linearFilter ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ci.anisotropyEnable = maxAniso > 1.0f ? VK_TRUE : VK_FALSE;
    ci.maxAnisotropy = maxAniso;
    ci.minLod = 0.0f;
    ci.maxLod = static_cast<float>(m_image.mipLevels());
    ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    VkResult r = vkCreateSampler(m_device, &ci, nullptr, &m_sampler);
    if (r != VK_SUCCESS) {
        LOG(Assets, Error, "Failed to create sampler: %d", (int)r);
        return false;
    }

    LOG(Assets, Trace, "Texture created: %ux%u, %u mips", m_image.width(), m_image.height(), m_image.mipLevels());
    return true;
}

void Texture::shutdown() { release(); }

void Texture::release() {
    if (m_sampler && m_device) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    m_image.shutdown();
}

} // namespace lmao
