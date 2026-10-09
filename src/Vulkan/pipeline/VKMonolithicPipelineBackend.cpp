#include<hgl/vk/pipeline/VKMonolithicPipelineBackend.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKDevice.h>

namespace hgl::graph
{
    MonolithicPipelineBackend &MonolithicPipelineBackend::Instance()
    {
        static MonolithicPipelineBackend backend;
        return backend;
    }

    GraphicsPipelineMode MonolithicPipelineBackend::GetMode() const
    {
        return GraphicsPipelineMode::Monolithic;
    }

    bool MonolithicPipelineBackend::Materialize(const FinalPipelineResolveRequest &request, VkPipeline &out_pipeline)
    {
        out_pipeline = VK_NULL_HANDLE;

        if(!request.device || !request.shader_stages)
            return false;

        // ── pipeline 只保留 shader 部分 ──
        // 所有可变渲染状态（剔除/深度/混合/线框/线宽/alpha-to-coverage 等）由
        // VK_EXT_extended_dynamic_state 1/2/3 动态设置（渲染侧 vkCmdSet* 应用材质配置），
        // 创建时一律给 Vulkan 要求的合法默认值结构。

        // 空顶点输入（mesh shader 唯一顶点路径，顶点数据走 SSBO）
        VkPipelineVertexInputStateCreateInfo vertex_input_state{};
        vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // mesh shader 管线中无 VS 输入组装，此值被忽略

        // viewport/scissor 动态——创建时给占位值
        VkViewport viewport{0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, {1u, 1u}};

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.depthClampEnable = VK_FALSE;
        rasterization.rasterizerDiscardEnable = VK_FALSE;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;   // 动态（EDS2）
        rasterization.cullMode = VK_CULL_MODE_BACK_BIT;     // 动态（EDS1）
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.depthBiasEnable = VK_FALSE;
        rasterization.lineWidth = 1.0f;                     // 动态

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample.alphaToCoverageEnable = VK_FALSE;       // 本引擎不支持 A2C（恒 FALSE）

        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_FALSE;           // 动态（EDS1）
        depth_stencil.depthWriteEnable = VK_FALSE;          // 动态（EDS1）
        depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;  // 动态（EDS1）
        depth_stencil.front.failOp = VK_STENCIL_OP_KEEP;
        depth_stencil.front.passOp = VK_STENCIL_OP_KEEP;
        depth_stencil.front.depthFailOp = VK_STENCIL_OP_KEEP;
        depth_stencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
        depth_stencil.back = depth_stencil.front;

        VkPipelineColorBlendAttachmentState blend_attachment{};
        blend_attachment.blendEnable = VK_FALSE;            // 动态（EDS2）
        blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;  // 动态（EDS2）

        VkPipelineColorBlendStateCreateInfo color_blend{};
        color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend.attachmentCount = 1;
        color_blend.pAttachments = &blend_attachment;

        // EDS 动态状态：pipeline 只留 shader，材质配置全部渲染侧 vkCmdSet* 应用
        //（VIEWPORT/SCISSOR/LINE_WIDTH 为 1.0 核心；其余 EDS 1/2/3 用 EXT 名——兼容任意 SDK 头）
        const VkDynamicState dynamic_states[] =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_CULL_MODE_EXT,
            VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,
            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
            VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,
            VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,
            VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,
            VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,
            VK_DYNAMIC_STATE_BLEND_CONSTANTS,   // 1.0 核心（非 EDS2 引入，无 EXT 名）
            VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
            VK_DYNAMIC_STATE_LINE_WIDTH,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(sizeof(dynamic_states) / sizeof(dynamic_states[0]));
        dynamic_state.pDynamicStates = dynamic_states;

        // Dynamic Rendering：renderPass=VK_NULL_HANDLE 时 pNext 必须含
        // VkPipelineRenderingCreateInfo（VUID-VkGraphicsPipelineCreateInfo-renderPass-06061）
        VkPipelineRenderingCreateInfo rendering_ci{};
        rendering_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_ci.colorAttachmentCount = request.frame_output.color_attachment_count;
        rendering_ci.pColorAttachmentFormats = request.frame_output.color_formats;
        rendering_ci.depthAttachmentFormat = request.frame_output.depth_stencil_format;
        // 只有深度格式确实带模板位时才声明模板附件。对 D16_UNORM / D32_SFLOAT 这类
        // 纯深度格式（shadow map 常用），stencilAttachmentFormat 必须为 UNDEFINED，
        // 否则与动态渲染时实际附件格式不符。
        rendering_ci.stencilAttachmentFormat = IsStencilFormat(request.frame_output.depth_stencil_format)
                                                   ? request.frame_output.depth_stencil_format
                                                   : VK_FORMAT_UNDEFINED;

        VkGraphicsPipelineCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        ci.stageCount = request.shader_stages->GetCount();
        ci.pStages = request.shader_stages->GetData();
        ci.pVertexInputState = &vertex_input_state;
        ci.pInputAssemblyState = &input_assembly;
        ci.pViewportState = &viewport_state;
        ci.pRasterizationState = &rasterization;
        ci.pMultisampleState = &multisample;
        ci.pDepthStencilState = &depth_stencil;
        ci.pColorBlendState = &color_blend;
        ci.pDynamicState = &dynamic_state;
        ci.layout = request.pipeline_layout;
        ci.renderPass = VK_NULL_HANDLE;   // Dynamic Rendering
        ci.subpass = 0;
        ci.pNext = &rendering_ci;

        if (request.device && request.device->IsDescriptorBufferActive())
            ci.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

        return vkCreateGraphicsPipelines(*request.device,
                                         request.pipeline_cache,
                                         1,
                                         &ci,
                                         nullptr,
                                         &out_pipeline) == VK_SUCCESS;
    }

    void MonolithicPipelineBackend::ClearCacheForDevice(VulkanDevice *)
    {
    }
}
