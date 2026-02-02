#include "vulkan/DescriptorManager.h"
#include "vulkan/VulkanUtils.h"
#include "core/Log.h"
#include <functional>

namespace lmao {

DescriptorManager::~DescriptorManager() { shutdown(); }

bool DescriptorManager::init(VkDevice device, uint32_t maxSets) {
    m_device = device;

    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSets * 4},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxSets * 8},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, maxSets * 2},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, maxSets * 2},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, maxSets},
    };

    VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets = maxSets;
    ci.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    ci.pPoolSizes = poolSizes.data();

    VK_CHECK(vkCreateDescriptorPool(m_device, &ci, nullptr, &m_pool));
    return true;
}

void DescriptorManager::shutdown() {
    for (auto& [hash, layout] : m_layoutCache)
        vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
    m_layoutCache.clear();

    if (m_pool) {
        vkDestroyDescriptorPool(m_device, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
}

VkDescriptorSetLayout DescriptorManager::getOrCreateLayout(
    const VkDescriptorSetLayoutBinding* bindings, uint32_t count) {
    // Hash the layout description
    size_t hash = 0;
    for (uint32_t i = 0; i < count; i++) {
        hash ^= std::hash<uint32_t>()(bindings[i].binding) << 1;
        hash ^= std::hash<uint32_t>()(bindings[i].descriptorType) << 3;
        hash ^= std::hash<uint32_t>()(bindings[i].descriptorCount) << 5;
        hash ^= std::hash<uint32_t>()(bindings[i].stageFlags) << 7;
    }

    auto it = m_layoutCache.find(hash);
    if (it != m_layoutCache.end()) return it->second;

    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = count;
    ci.pBindings = bindings;

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(m_device, &ci, nullptr, &layout));
    m_layoutCache[hash] = layout;
    return layout;
}

VkDescriptorSet DescriptorManager::allocate(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = m_pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &layout;

    VkDescriptorSet set;
    VK_CHECK(vkAllocateDescriptorSets(m_device, &ai, &set));
    return set;
}

void DescriptorManager::writeBuffer(VkDevice device, VkDescriptorSet set, uint32_t binding,
                                     VkBuffer buffer, VkDeviceSize size,
                                     VkDescriptorType type, VkDeviceSize offset) {
    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = buffer;
    bufInfo.offset = offset;
    bufInfo.range = size;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &bufInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void DescriptorManager::writeImage(VkDevice device, VkDescriptorSet set, uint32_t binding,
                                    VkImageView view, VkSampler sampler,
                                    VkImageLayout layout, VkDescriptorType type) {
    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageView = view;
    imgInfo.sampler = sampler;
    imgInfo.imageLayout = layout;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void DescriptorManager::writeStorageImage(VkDevice device, VkDescriptorSet set, uint32_t binding,
                                           VkImageView view, VkImageLayout layout) {
    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageView = view;
    imgInfo.imageLayout = layout;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

} // namespace lmao
