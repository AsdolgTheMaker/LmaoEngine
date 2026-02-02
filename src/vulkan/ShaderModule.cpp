#include "vulkan/ShaderModule.h"
#include "vulkan/VulkanUtils.h"
#include "core/Log.h"
#include <fstream>

namespace lmao {

ShaderModule::~ShaderModule() { shutdown(); }

ShaderModule::ShaderModule(ShaderModule&& o) noexcept
    : m_device(o.m_device), m_module(o.m_module) {
    o.m_module = VK_NULL_HANDLE;
}

ShaderModule& ShaderModule::operator=(ShaderModule&& o) noexcept {
    if (this != &o) {
        shutdown();
        m_device = o.m_device;
        m_module = o.m_module;
        o.m_module = VK_NULL_HANDLE;
    }
    return *this;
}

bool ShaderModule::loadFromFile(VkDevice device, const std::string& path) {
    m_device = device;

    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG(Pipeline, Error, "Failed to open shader file: %s", path.c_str());
        return false;
    }

    size_t size = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> code(size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(size));

    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = size;
    ci.pCode = code.data();

    VK_CHECK(vkCreateShaderModule(m_device, &ci, nullptr, &m_module));
    LOG(Pipeline, Debug, "Shader loaded: %s (%zu bytes)", path.c_str(), size);
    return true;
}

void ShaderModule::shutdown() {
    if (m_module && m_device) {
        vkDestroyShaderModule(m_device, m_module, nullptr);
        m_module = VK_NULL_HANDLE;
    }
}

VkPipelineShaderStageCreateInfo ShaderModule::stageInfo(VkShaderStageFlagBits stage, const char* entry) const {
    VkPipelineShaderStageCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    info.stage = stage;
    info.module = m_module;
    info.pName = entry;
    return info;
}

} // namespace lmao
