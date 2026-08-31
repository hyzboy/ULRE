#include<hgl/vk/pipeline/VKPipelineResolver.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/thread/ThreadMutex.h>
#include<hgl/util/hash/FNV1a.h>
#include<hgl/log/Log.h>
#include<cstring>

namespace hgl::graph
{
    namespace
    {
        uint64_t HashBytes(const void *data, const size_t size)
        {
            hgl::hash::FNV1aHasher64 h;
            h.AppendBytes(data, size);
            return h;
        }

        uint64_t HashShaderStages(const ShaderStageCreateInfoList *shader_stages)
        {
            if(!shader_stages || shader_stages->IsEmpty())
                return 0;

            hgl::hash::FNV1aHasher64 h;

            const uint count = shader_stages->GetCount();
            const VkPipelineShaderStageCreateInfo *stages = shader_stages->GetData();
            h << count;

            for(uint i = 0; i < count; ++i)
            {
                h << stages[i].stage
                  << (uint64_t)(uintptr_t)stages[i].module;
                if(stages[i].pName)
                    h << stages[i].pName;
            }

            return h;
        }

        struct FinalPipelineCacheEntry
        {
            VulkanDevice *device = nullptr;
            VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
            FinalPipelineKey key{};
            VkPipeline pipeline = VK_NULL_HANDLE;
            uint32_t references = 0;

            bool operator == (const FinalPipelineCacheEntry &rhs) const
            {
                return device == rhs.device
                    && pipeline_layout == rhs.pipeline_layout
                    && key == rhs.key
                    && pipeline == rhs.pipeline
                    && references == rhs.references;
            }
        };

        ValueArray<FinalPipelineCacheEntry> g_pipeline_cache;

        struct PipelineResolveStats
        {
            uint64_t requests = 0;
            uint64_t invalid_request = 0;
            uint64_t incomplete_key = 0;
            uint64_t final_cache_hit = 0;
            uint64_t final_cache_miss = 0;
            uint64_t materialize_success = 0;
            uint64_t materialize_failed = 0;
        };

        enum class ResolveError:uint8
        {
            None = 0,
            MissingDevice,
            MissingShaderStages,
            MissingPipelineLayout,
            MissingColorAttachmentConfig
        };

        PipelineResolveStats g_resolve_stats;
        ThreadMutex g_resolver_mutex;

        const char *ResolveErrorText(const ResolveError error)
        {
            switch(error)
            {
            case ResolveError::MissingDevice: return "missing device";
            case ResolveError::MissingShaderStages: return "missing shader_stages";
            case ResolveError::MissingPipelineLayout: return "missing pipeline_layout";
            case ResolveError::MissingColorAttachmentConfig: return "missing frame_output color attachments";
            default: return "none";
            }
        }

        ResolveError ValidateResolveRequest(const FinalPipelineResolveRequest &request)
        {
            if(!request.device)
                return ResolveError::MissingDevice;

            if(!request.shader_stages || request.shader_stages->IsEmpty())
                return ResolveError::MissingShaderStages;

            if(request.pipeline_layout == VK_NULL_HANDLE)
                return ResolveError::MissingPipelineLayout;

            if(request.frame_output.color_attachment_count == 0 || !request.frame_output.color_formats)
                return ResolveError::MissingColorAttachmentConfig;

            return ResolveError::None;
        }

        void LogResolveStatsIfNeeded()
        {
            if((g_resolve_stats.requests % 64u) != 0u)
                return;

            GLogInfo("[PipelineResolver] req=%llu invalid=%llu incomplete_key=%llu final_cache(h/m)=%llu/%llu materialize(ok/fail)=%llu/%llu",
                     (unsigned long long)g_resolve_stats.requests,
                     (unsigned long long)g_resolve_stats.invalid_request,
                     (unsigned long long)g_resolve_stats.incomplete_key,
                     (unsigned long long)g_resolve_stats.final_cache_hit,
                     (unsigned long long)g_resolve_stats.final_cache_miss,
                     (unsigned long long)g_resolve_stats.materialize_success,
                     (unsigned long long)g_resolve_stats.materialize_failed);
        }

        ValueArray<FinalPipelineCacheEntry> &GetFinalPipelineCache()
        {
            return g_pipeline_cache;
        }

        bool TryGetCachedFinalPipeline(
                                      const FinalPipelineResolveRequest &request,
                                      const FinalPipelineKey &key,
                                      VkPipeline &out_pipeline)
        {
            if(!request.device || request.pipeline_layout == VK_NULL_HANDLE)
                return false;

            ValueArray<FinalPipelineCacheEntry> &cache = GetFinalPipelineCache();

            const int count = cache.GetCount();
            for(int i = 0; i < count; ++i)
            {
                FinalPipelineCacheEntry &entry = cache[i];
                if(entry.device != request.device)
                    continue;
                if(entry.pipeline_layout != request.pipeline_layout)
                    continue;
                if(!(entry.key == key))
                    continue;
                if(entry.pipeline == VK_NULL_HANDLE)
                    continue;

                out_pipeline = entry.pipeline;
                ++entry.references;
                return true;
            }

            return false;
        }

        void CacheFinalPipeline(
                                const FinalPipelineResolveRequest &request,
                                const FinalPipelineKey &key,
                                VkPipeline pipeline)
        {
            if(!request.device || request.pipeline_layout == VK_NULL_HANDLE || pipeline == VK_NULL_HANDLE)
                return;

            FinalPipelineCacheEntry entry{};
            entry.device = request.device;
            entry.pipeline_layout = request.pipeline_layout;
            entry.key = key;
            entry.pipeline = pipeline;
            entry.references = 1;
            GetFinalPipelineCache().Add(entry);
        }

    }

    bool PipelineResolver::BuildFinalPipelineKey(const FinalPipelineResolveRequest &request, FinalPipelineKey &out_key)
    {
        out_key = {};

        out_key.shader_stages_hash = HashShaderStages(request.shader_stages);

        out_key.color_attachment_count = request.frame_output.color_attachment_count;
        out_key.color_formats_hash = (request.frame_output.color_formats && request.frame_output.color_attachment_count > 0)
                                   ? HashBytes(request.frame_output.color_formats, sizeof(VkFormat) * request.frame_output.color_attachment_count)
                                   : 0;
        out_key.depth_stencil_format = request.frame_output.depth_stencil_format;

        return true;
    }

    bool PipelineResolver::HasCompleteFinalKey(const FinalPipelineKey &key)
    {
        if(key.shader_stages_hash == 0)
            return false;

        if(key.color_attachment_count == 0)
            return false;

        return true;
    }

    bool PipelineResolver::MaterializePipeline(const FinalPipelineResolveRequest &request, VkPipeline &out_pipeline)
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
        multisample.alphaToCoverageEnable = VK_FALSE;       // 动态（EDS3）

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
            VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT,
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
        rendering_ci.stencilAttachmentFormat = request.frame_output.depth_stencil_format;

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

        return vkCreateGraphicsPipelines(*request.device,
                                         request.pipeline_cache,
                                         1,
                                         &ci,
                                         nullptr,
                                         &out_pipeline) == VK_SUCCESS;
    }

    bool PipelineResolver::ResolveFinalPipeline(const FinalPipelineResolveRequest &request, FinalPipelineResolveResult &out_result)
    {
        ThreadMutexLock resolver_lock(&g_resolver_mutex);

        out_result = {};
        ++g_resolve_stats.requests;

        const ResolveError request_error = ValidateResolveRequest(request);
        if(request_error != ResolveError::None)
        {
            ++g_resolve_stats.invalid_request;

            GLogError("[PipelineResolver] Resolve failed: %s", ResolveErrorText(request_error));
            LogResolveStatsIfNeeded();
            return false;
        }

        BuildFinalPipelineKey(request, out_result.key);
        if(!HasCompleteFinalKey(out_result.key))
        {
            ++g_resolve_stats.incomplete_key;
            GLogError("[PipelineResolver] Resolve failed: incomplete FinalPipelineKey");
            LogResolveStatsIfNeeded();
            return false;
        }

        if(TryGetCachedFinalPipeline(request, out_result.key, out_result.pipeline))
        {
            ++g_resolve_stats.final_cache_hit;
            LogResolveStatsIfNeeded();
            return true;
        }

        ++g_resolve_stats.final_cache_miss;

        if(!MaterializePipeline(request, out_result.pipeline))
        {
            ++g_resolve_stats.materialize_failed;
            GLogError("[PipelineResolver] Materialize failed");
            LogResolveStatsIfNeeded();
            return false;
        }
        ++g_resolve_stats.materialize_success;
        CacheFinalPipeline(request, out_result.key, out_result.pipeline);
        LogResolveStatsIfNeeded();
        return true;
    }

    void PipelineResolver::ReleaseFinalPipeline(VulkanDevice *device, VkPipeline pipeline)
    {
        ThreadMutexLock resolver_lock(&g_resolver_mutex);

        if(!device || pipeline == VK_NULL_HANDLE)
            return;

        ValueArray<FinalPipelineCacheEntry> &cache = GetFinalPipelineCache();

        for(int i = cache.GetCount() - 1; i >= 0; --i)
        {
            if(cache[i].device == device && cache[i].pipeline == pipeline)
            {
                if(cache[i].references > 1)
                {
                    --cache[i].references;
                    continue;
                }

                cache.Delete(i);
            }
        }
    }

    void PipelineResolver::ClearCacheForDevice(VulkanDevice *device)
    {
        ThreadMutexLock resolver_lock(&g_resolver_mutex);

        if(!device)
            return;

        ValueArray<FinalPipelineCacheEntry> &cache = GetFinalPipelineCache();

        int write = 0;
        const int count = cache.GetCount();
        for(int i = 0; i < count; ++i)
        {
            if(cache[i].device != device)
            {
                if(write != i)
                    cache[write] = cache[i];
                ++write;
            }
        }
        while(cache.GetCount() > write)
            cache.Delete(cache.GetCount() - 1);

        GLogInfo("[PipelineResolver] Cleared caches for device %p. Remaining: %d",
                 (void *)device,
                 cache.GetCount());
    }


}//namespace hgl::graph
