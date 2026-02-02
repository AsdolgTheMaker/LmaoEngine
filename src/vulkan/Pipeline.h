#pragma once
#include <volk.h>
#include <vector>
#include <cstdint>

namespace lmao {

// Graphics pipeline builder with sensible defaults
class PipelineBuilder {
public:
    PipelineBuilder();

    PipelineBuilder& addShaderStage(VkPipelineShaderStageCreateInfo stage);
    PipelineBuilder& setVertexInput(const VkVertexInputBindingDescription* bindings, uint32_t bindingCount,
                                     const VkVertexInputAttributeDescription* attrs, uint32_t attrCount);
    PipelineBuilder& setTopology(VkPrimitiveTopology topology);
    PipelineBuilder& setPolygonMode(VkPolygonMode mode);
    PipelineBuilder& setCullMode(VkCullModeFlags mode, VkFrontFace front = VK_FRONT_FACE_COUNTER_CLOCKWISE);
    PipelineBuilder& setDepthTest(bool enable, bool write = true, VkCompareOp op = VK_COMPARE_OP_GREATER_OR_EQUAL);
    PipelineBuilder& setDepthBias(bool enable, float constantFactor = 0.0f, float slopeFactor = 0.0f);
    PipelineBuilder& setColorBlendAttachment(uint32_t count, bool blendEnable = false);
    PipelineBuilder& setMultisample(VkSampleCountFlagBits samples);
    PipelineBuilder& setDynamicStates(const std::vector<VkDynamicState>& states);
    PipelineBuilder& setLayout(VkPipelineLayout layout);

    // Dynamic rendering (Vulkan 1.3)
    PipelineBuilder& setColorFormats(const std::vector<VkFormat>& formats);
    PipelineBuilder& setDepthFormat(VkFormat format);

    VkPipeline build(VkDevice device);

private:
    std::vector<VkPipelineShaderStageCreateInfo> m_stages;
    VkPipelineVertexInputStateCreateInfo m_vertexInput{};
    VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{};
    VkPipelineRasterizationStateCreateInfo m_rasterization{};
    VkPipelineMultisampleStateCreateInfo m_multisample{};
    VkPipelineDepthStencilStateCreateInfo m_depthStencil{};
    std::vector<VkPipelineColorBlendAttachmentState> m_blendAttachments;
    VkPipelineColorBlendStateCreateInfo m_colorBlend{};
    std::vector<VkDynamicState> m_dynamicStates;
    VkPipelineDynamicStateCreateInfo m_dynamicState{};
    VkPipelineLayout m_layout = VK_NULL_HANDLE;

    // Dynamic rendering
    std::vector<VkFormat> m_colorFormats;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
    VkPipelineRenderingCreateInfo m_renderingInfo{};

    std::vector<VkVertexInputBindingDescription> m_bindings;
    std::vector<VkVertexInputAttributeDescription> m_attributes;
};

} // namespace lmao
