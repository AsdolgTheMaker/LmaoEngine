#pragma once
#include <volk.h>
#include <string>
#include <vector>

namespace lmao {

class ShaderModule {
public:
    ShaderModule() = default;
    ~ShaderModule();

    ShaderModule(ShaderModule&& o) noexcept;
    ShaderModule& operator=(ShaderModule&& o) noexcept;
    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    bool loadFromFile(VkDevice device, const std::string& path);
    void shutdown();

    VkShaderModule handle() const { return m_module; }
    VkPipelineShaderStageCreateInfo stageInfo(VkShaderStageFlagBits stage, const char* entry = "main") const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkShaderModule m_module = VK_NULL_HANDLE;
};

} // namespace lmao
