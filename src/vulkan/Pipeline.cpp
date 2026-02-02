#include "vulkan/Pipeline.h"
#include "vulkan/VulkanUtils.h"
#include "core/Log.h"

namespace lmao {

PipelineBuilder::PipelineBuilder() {
    m_vertexInput = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    m_inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    m_rasterization = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    m_rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    m_rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    m_rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    m_rasterization.lineWidth = 1.0f;

    m_multisample = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    m_multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    m_depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    m_depthStencil.depthTestEnable = VK_TRUE;
    m_depthStencil.depthWriteEnable = VK_TRUE;
    m_depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // reversed-Z

    // Default: one non-blended attachment
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    m_blendAttachments.push_back(blend);

    m_colorBlend = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};

    m_dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    m_dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};

    m_renderingInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
}

PipelineBuilder& PipelineBuilder::addShaderStage(VkPipelineShaderStageCreateInfo stage) {
    m_stages.push_back(stage);
    return *this;
}

PipelineBuilder& PipelineBuilder::setVertexInput(
    const VkVertexInputBindingDescription* bindings, uint32_t bindingCount,
    const VkVertexInputAttributeDescription* attrs, uint32_t attrCount) {
    m_bindings.assign(bindings, bindings + bindingCount);
    m_attributes.assign(attrs, attrs + attrCount);
    return *this;
}

PipelineBuilder& PipelineBuilder::setTopology(VkPrimitiveTopology topology) {
    m_inputAssembly.topology = topology;
    return *this;
}

PipelineBuilder& PipelineBuilder::setPolygonMode(VkPolygonMode mode) {
    m_rasterization.polygonMode = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::setCullMode(VkCullModeFlags mode, VkFrontFace front) {
    m_rasterization.cullMode = mode;
    m_rasterization.frontFace = front;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthTest(bool enable, bool write, VkCompareOp op) {
    m_depthStencil.depthTestEnable = enable ? VK_TRUE : VK_FALSE;
    m_depthStencil.depthWriteEnable = write ? VK_TRUE : VK_FALSE;
    m_depthStencil.depthCompareOp = op;
    return *this;
}

PipelineBuilder& PipelineBuilder::setColorBlendAttachment(uint32_t count, bool blendEnable) {
    m_blendAttachments.clear();
    for (uint32_t i = 0; i < count; i++) {
        VkPipelineColorBlendAttachmentState blend{};
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blend.blendEnable = blendEnable ? VK_TRUE : VK_FALSE;
        if (blendEnable) {
            blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.colorBlendOp = VK_BLEND_OP_ADD;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            blend.alphaBlendOp = VK_BLEND_OP_ADD;
        }
        m_blendAttachments.push_back(blend);
    }
    return *this;
}

PipelineBuilder& PipelineBuilder::setDynamicStates(const std::vector<VkDynamicState>& states) {
    m_dynamicStates = states;
    return *this;
}

PipelineBuilder& PipelineBuilder::setLayout(VkPipelineLayout layout) {
    m_layout = layout;
    return *this;
}

PipelineBuilder& PipelineBuilder::setColorFormats(const std::vector<VkFormat>& formats) {
    m_colorFormats = formats;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthFormat(VkFormat format) {
    m_depthFormat = format;
    return *this;
}

VkPipeline PipelineBuilder::build(VkDevice device) {
    m_vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(m_bindings.size());
    m_vertexInput.pVertexBindingDescriptions = m_bindings.data();
    m_vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_attributes.size());
    m_vertexInput.pVertexAttributeDescriptions = m_attributes.data();

    m_colorBlend.attachmentCount = static_cast<uint32_t>(m_blendAttachments.size());
    m_colorBlend.pAttachments = m_blendAttachments.data();

    m_dynamicState.dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
    m_dynamicState.pDynamicStates = m_dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Dynamic rendering
    m_renderingInfo.colorAttachmentCount = static_cast<uint32_t>(m_colorFormats.size());
    m_renderingInfo.pColorAttachmentFormats = m_colorFormats.data();
    m_renderingInfo.depthAttachmentFormat = m_depthFormat;

    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.pNext = &m_renderingInfo;
    ci.stageCount = static_cast<uint32_t>(m_stages.size());
    ci.pStages = m_stages.data();
    ci.pVertexInputState = &m_vertexInput;
    ci.pInputAssemblyState = &m_inputAssembly;
    ci.pViewportState = &viewportState;
    ci.pRasterizationState = &m_rasterization;
    ci.pMultisampleState = &m_multisample;
    ci.pDepthStencilState = &m_depthStencil;
    ci.pColorBlendState = &m_colorBlend;
    ci.pDynamicState = &m_dynamicState;
    ci.layout = m_layout;
    ci.renderPass = VK_NULL_HANDLE; // dynamic rendering

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline));
    LOG(Pipeline, Debug, "Graphics pipeline created: %u stages, %u color attachments",
        (uint32_t)m_stages.size(), (uint32_t)m_colorFormats.size());
    return pipeline;
}

} // namespace lmao
