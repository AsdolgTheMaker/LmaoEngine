#pragma once
#include "assets/Texture.h"
#include "math/MathUtils.h"
#include <memory>
#include <string>

namespace lmao {

class VulkanContext;
class CommandPool;

class TextureLoader {
public:
    static std::shared_ptr<Texture> load(VulkanContext& ctx, CommandPool& cmdPool,
                                          const std::string& path,
                                          bool genMipmaps = true, bool sRGB = true);

    static std::shared_ptr<Texture> createSolidColor(VulkanContext& ctx, CommandPool& cmdPool,
                                                      const vec4& color, bool sRGB = true);

    static std::shared_ptr<Texture> createCheckerboard(VulkanContext& ctx, CommandPool& cmdPool,
                                                        uint32_t size = 256, uint32_t tileSize = 32,
                                                        const vec4& color1 = vec4(0.9f, 0.9f, 0.9f, 1.0f),
                                                        const vec4& color2 = vec4(0.3f, 0.3f, 0.3f, 1.0f));

private:
    static void generateMipmaps(VkCommandBuffer cmd, VkImage image, VkFormat format,
                                 int32_t width, int32_t height, uint32_t mipLevels);
};

} // namespace lmao
