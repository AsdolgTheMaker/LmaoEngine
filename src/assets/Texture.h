#pragma once
#include "vulkan/Image.h"
#include <volk.h>

namespace lmao {

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(Texture&& o) noexcept;
    Texture& operator=(Texture&& o) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool initFromImage(VkDevice device, Image&& image, bool linearFilter = true, float maxAniso = 16.0f);
    void shutdown();

    VkImageView imageView() const { return m_image.view(); }
    VkSampler sampler() const { return m_sampler; }
    const Image& image() const { return m_image; }

private:
    void release();

    VkDevice m_device = VK_NULL_HANDLE;
    Image m_image;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

} // namespace lmao
