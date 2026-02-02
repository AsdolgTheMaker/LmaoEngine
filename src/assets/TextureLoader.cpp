#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "assets/TextureLoader.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/CommandPool.h"
#include "vulkan/Buffer.h"
#include "core/Log.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace lmao {

std::shared_ptr<Texture> TextureLoader::load(VulkanContext& ctx, CommandPool& cmdPool,
                                              const std::string& path,
                                              bool genMipmaps, bool sRGB) {
    int w, h, channels;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        LOG(Assets, Error, "Failed to load texture: %s", path.c_str());
        return nullptr;
    }

    VkFormat format = sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    uint32_t mipLevels = genMipmaps
        ? static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1
        : 1;

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(w) * h * 4;

    // Staging buffer
    Buffer staging;
    staging.init(ctx.allocator(), imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    staging.upload(pixels, imageSize);
    stbi_image_free(pixels);

    // Create image
    Image::CreateInfo imgCI{};
    imgCI.width = static_cast<uint32_t>(w);
    imgCI.height = static_cast<uint32_t>(h);
    imgCI.format = format;
    imgCI.mipLevels = mipLevels;
    imgCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (genMipmaps) imgCI.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    Image image;
    image.init(ctx.allocator(), ctx.device(), imgCI);

    cmdPool.submitImmediate(ctx.graphicsQueue(), [&](VkCommandBuffer cmd) {
        // Transition to transfer dst
        Image::transitionLayout(cmd, image.handle(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);

        // Copy buffer to mip 0
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
        vkCmdCopyBufferToImage(cmd, staging.handle(), image.handle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        if (genMipmaps && mipLevels > 1) {
            generateMipmaps(cmd, image.handle(), format, w, h, mipLevels);
        } else {
            Image::transitionLayout(cmd, image.handle(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
        }
    });
    staging.shutdown();

    auto tex = std::make_shared<Texture>();
    tex->initFromImage(ctx.device(), std::move(image));
    LOG(Assets, Info, "Texture loaded: %s (%dx%d, %u mips)", path.c_str(), w, h, mipLevels);
    return tex;
}

std::shared_ptr<Texture> TextureLoader::createSolidColor(VulkanContext& ctx, CommandPool& cmdPool,
                                                          const vec4& color, bool sRGB) {
    uint8_t r = static_cast<uint8_t>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f);
    uint8_t g = static_cast<uint8_t>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
    uint8_t b = static_cast<uint8_t>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);
    uint8_t a = static_cast<uint8_t>(std::clamp(color.a, 0.0f, 1.0f) * 255.0f);
    uint8_t pixel[4] = {r, g, b, a};

    VkFormat format = sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    Buffer staging;
    staging.init(ctx.allocator(), 4,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    staging.upload(pixel, 4);

    Image::CreateInfo imgCI{};
    imgCI.width = 1;
    imgCI.height = 1;
    imgCI.format = format;
    imgCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    Image image;
    image.init(ctx.allocator(), ctx.device(), imgCI);

    cmdPool.submitImmediate(ctx.graphicsQueue(), [&](VkCommandBuffer cmd) {
        Image::transitionLayout(cmd, image.handle(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {1, 1, 1};
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

std::shared_ptr<Texture> TextureLoader::createCheckerboard(VulkanContext& ctx, CommandPool& cmdPool,
                                                            uint32_t size, uint32_t tileSize,
                                                            const vec4& color1, const vec4& color2) {
    std::vector<uint8_t> pixels(size * size * 4);
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            bool white = ((x / tileSize) + (y / tileSize)) % 2 == 0;
            const vec4& c = white ? color1 : color2;
            uint32_t idx = (y * size + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(c.r * 255.0f);
            pixels[idx + 1] = static_cast<uint8_t>(c.g * 255.0f);
            pixels[idx + 2] = static_cast<uint8_t>(c.b * 255.0f);
            pixels[idx + 3] = static_cast<uint8_t>(c.a * 255.0f);
        }
    }

    VkDeviceSize imageSize = size * size * 4;
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(size))) + 1;

    Buffer staging;
    staging.init(ctx.allocator(), imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    staging.upload(pixels.data(), imageSize);

    Image::CreateInfo imgCI{};
    imgCI.width = size;
    imgCI.height = size;
    imgCI.format = VK_FORMAT_R8G8B8A8_SRGB;
    imgCI.mipLevels = mipLevels;
    imgCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    Image image;
    image.init(ctx.allocator(), ctx.device(), imgCI);

    cmdPool.submitImmediate(ctx.graphicsQueue(), [&](VkCommandBuffer cmd) {
        Image::transitionLayout(cmd, image.handle(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {size, size, 1};
        vkCmdCopyBufferToImage(cmd, staging.handle(), image.handle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        generateMipmaps(cmd, image.handle(), VK_FORMAT_R8G8B8A8_SRGB,
            static_cast<int32_t>(size), static_cast<int32_t>(size), mipLevels);
    });
    staging.shutdown();

    auto tex = std::make_shared<Texture>();
    tex->initFromImage(ctx.device(), std::move(image));
    LOG(Assets, Debug, "Generated checkerboard texture: %ux%u, tile=%u", size, size, tileSize);
    return tex;
}

void TextureLoader::generateMipmaps(VkCommandBuffer cmd, VkImage image, VkFormat,
                                     int32_t width, int32_t height, uint32_t mipLevels) {
    int32_t mipW = width, mipH = height;

    for (uint32_t i = 1; i < mipLevels; i++) {
        // Transition mip i-1 from TRANSFER_DST to TRANSFER_SRC
        VkImageMemoryBarrier2 toSrc{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        toSrc.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        toSrc.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        toSrc.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        toSrc.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        toSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSrc.image = image;
        toSrc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1};

        VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &toSrc;
        vkCmdPipelineBarrier2(cmd, &dep);

        int32_t nextW = std::max(mipW / 2, 1);
        int32_t nextH = std::max(mipH / 2, 1);

        VkImageBlit2 blit{VK_STRUCTURE_TYPE_IMAGE_BLIT_2};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipW, mipH, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1};
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {nextW, nextH, 1};

        VkBlitImageInfo2 blitInfo{VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2};
        blitInfo.srcImage = image;
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.dstImage = image;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blit;
        blitInfo.filter = VK_FILTER_LINEAR;

        vkCmdBlitImage2(cmd, &blitInfo);

        mipW = nextW;
        mipH = nextH;
    }

    // Transition last mip from TRANSFER_DST to SHADER_READ_ONLY
    // Transition all other mips from TRANSFER_SRC to SHADER_READ_ONLY
    std::vector<VkImageMemoryBarrier2> barriers(mipLevels);
    for (uint32_t i = 0; i < mipLevels; i++) {
        barriers[i] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barriers[i].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barriers[i].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barriers[i].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        barriers[i].image = image;
        barriers[i].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1};

        if (i < mipLevels - 1) {
            barriers[i].srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            barriers[i].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        } else {
            barriers[i].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barriers[i].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }
        barriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    dep.pImageMemoryBarriers = barriers.data();
    vkCmdPipelineBarrier2(cmd, &dep);
}

} // namespace lmao
