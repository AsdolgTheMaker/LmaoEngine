#pragma once
#include <volk.h>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace lmao {

class DescriptorManager {
public:
    DescriptorManager() = default;
    ~DescriptorManager();

    DescriptorManager(const DescriptorManager&) = delete;
    DescriptorManager& operator=(const DescriptorManager&) = delete;

    bool init(VkDevice device, uint32_t maxSets = 1000);
    void shutdown();

    // Create or retrieve a cached layout
    VkDescriptorSetLayout getOrCreateLayout(
        const VkDescriptorSetLayoutBinding* bindings, uint32_t count);

    // Allocate a descriptor set
    VkDescriptorSet allocate(VkDescriptorSetLayout layout);

    // Write helpers
    static void writeBuffer(VkDevice device, VkDescriptorSet set, uint32_t binding,
                            VkBuffer buffer, VkDeviceSize size,
                            VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VkDeviceSize offset = 0);

    static void writeImage(VkDevice device, VkDescriptorSet set, uint32_t binding,
                           VkImageView view, VkSampler sampler,
                           VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    static void writeStorageImage(VkDevice device, VkDescriptorSet set, uint32_t binding,
                                  VkImageView view,
                                  VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    std::unordered_map<size_t, VkDescriptorSetLayout> m_layoutCache;
};

} // namespace lmao
