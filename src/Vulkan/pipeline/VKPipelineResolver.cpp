#include<hgl/vk/pipeline/VKPipelineResolver.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
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

        uint64_t HashGeometryVertexFormat(const GeometryVertexFormat *gvf)
        {
            if(!gvf)
                return 0;

            const uint32_t count = gvf->GetCount();
            if(count == 0)
                return 0;

            hgl::hash::FNV1aHasher64 h;

            h << count;

            for(uint32_t i=0;i<count;i++)
            {
                const GeometryVertexAttributeFormat *attr = gvf->Get(i);
                if(!attr)
                    continue;

                h << attr->semantic
                  << attr->format
                  << attr->vec_size
                  << attr->stride;
            }

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

        uint64_t HashPreRasterConfig(const PipelineData *pd, PrimitiveType primitive_type)
        {
            if(!pd)
                return 0;

            hgl::hash::FNV1aHasher64 h;

            h << static_cast<uint32_t>(primitive_type);
            if(pd->rasterization)
            {
                const VkPipelineRasterizationStateCreateInfo &rs = *pd->rasterization;
                h << rs.flags
                  << rs.depthClampEnable
                  << rs.rasterizerDiscardEnable
                  << rs.polygonMode
                  << rs.cullMode
                  << rs.frontFace
                  << rs.depthBiasEnable
                  << rs.depthBiasConstantFactor
                  << rs.depthBiasClamp
                  << rs.depthBiasSlopeFactor
                  << rs.lineWidth;
            }
            h << pd->viewport_state.viewportCount
              << pd->viewport_state.scissorCount
              << pd->dynamic_state.dynamicStateCount;
            if(pd->dynamic_state.dynamicStateCount > 0 && pd->dynamic_state.pDynamicStates)
                h.AppendBytes(pd->dynamic_state.pDynamicStates,
                              sizeof(VkDynamicState) * pd->dynamic_state.dynamicStateCount);
            return h;
        }

        uint64_t HashFragmentOutputState(const PipelineData *pd)
        {
            if(!pd)
                return 0;

            hgl::hash::FNV1aHasher64 h;

            const bool has_multisample = pd->multi_sample != nullptr;
            h << has_multisample;
            if(has_multisample)
            {
                const VkPipelineMultisampleStateCreateInfo *ms = pd->multi_sample;
                h << ms->flags
                  << ms->rasterizationSamples
                  << ms->sampleShadingEnable
                  << ms->minSampleShading
                  << ms->alphaToCoverageEnable
                  << ms->alphaToOneEnable;

                const uint32_t sample_count = static_cast<uint32_t>(ms->rasterizationSamples);
                const uint32_t sample_mask_word_count = (sample_count + 31u) / 32u;
                h << sample_mask_word_count;

                if(sample_mask_word_count > 0 && ms->pSampleMask)
                    h.AppendBytes(ms->pSampleMask, sizeof(VkSampleMask) * sample_mask_word_count);
            }

            const bool has_depth_stencil = pd->depth_stencil != nullptr;
            h << has_depth_stencil;
            if(has_depth_stencil)
            {
                const VkPipelineDepthStencilStateCreateInfo *ds = pd->depth_stencil;
                h << ds->flags
                  << ds->depthTestEnable
                  << ds->depthWriteEnable
                  << ds->depthCompareOp
                  << ds->depthBoundsTestEnable
                  << ds->stencilTestEnable
                  << ds->front.failOp
                  << ds->front.passOp
                  << ds->front.depthFailOp
                  << ds->front.compareOp
                  << ds->front.compareMask
                  << ds->front.writeMask
                  << ds->front.reference
                  << ds->back.failOp
                  << ds->back.passOp
                  << ds->back.depthFailOp
                  << ds->back.compareOp
                  << ds->back.compareMask
                  << ds->back.writeMask
                  << ds->back.reference
                  << ds->minDepthBounds
                  << ds->maxDepthBounds;
            }

            const bool has_color_blend = pd->color_blend != nullptr;
            h << has_color_blend;
            if(pd->color_blend)
            {
                h << pd->color_blend->attachmentCount
                  << pd->color_blend->flags
                  << pd->color_blend->logicOpEnable
                  << pd->color_blend->logicOp
                  << pd->color_blend->blendConstants[0]
                  << pd->color_blend->blendConstants[1]
                  << pd->color_blend->blendConstants[2]
                  << pd->color_blend->blendConstants[3];
            }
            if(pd->color_blend_attachments && pd->color_blend)
                h.AppendBytes(pd->color_blend_attachments, sizeof(VkPipelineColorBlendAttachmentState) * pd->color_blend->attachmentCount);
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

        ValueArray<FinalPipelineCacheEntry> g_monolithic_pipeline_cache;

        struct PipelineResolveStats
        {
            uint64_t requests = 0;
            uint64_t invalid_request = 0;
            uint64_t incomplete_key = 0;
            uint64_t fo_mismatch = 0;
            uint64_t final_cache_hit = 0;
            uint64_t final_cache_miss = 0;
            uint64_t materialize_success = 0;
            uint64_t materialize_failed = 0;
        };

        enum class ResolveError:uint8
        {
            None = 0,
            MissingDevice,
            MissingPipelineData,
            MissingShaderStages,
            MissingPipelineLayout,
            MissingColorAttachmentConfig,
            FOColorBlendAttachmentMismatch
        };

        PipelineResolveStats g_resolve_stats;
        ThreadMutex g_resolver_mutex;

        const char *ResolveErrorText(const ResolveError error)
        {
            switch(error)
            {
            case ResolveError::MissingDevice: return "missing device";
            case ResolveError::MissingPipelineData: return "missing pipeline_data";
            case ResolveError::MissingShaderStages: return "missing shader_stages";
            case ResolveError::MissingPipelineLayout: return "missing pipeline_layout";
            case ResolveError::MissingColorAttachmentConfig: return "missing frame_output color attachments";
            case ResolveError::FOColorBlendAttachmentMismatch: return "fo mismatch: color_blend attachment count differs from frame_output";
            default: return "none";
            }
        }

        ResolveError ValidateResolveRequest(const FinalPipelineResolveRequest &request)
        {
            if(!request.device)
                return ResolveError::MissingDevice;

            if(!request.pipeline_data)
                return ResolveError::MissingPipelineData;

            if(!request.shader_stages || request.shader_stages->IsEmpty())
                return ResolveError::MissingShaderStages;

            if(request.pipeline_layout == VK_NULL_HANDLE)
                return ResolveError::MissingPipelineLayout;

            if(request.frame_output.color_attachment_count == 0 || !request.frame_output.color_formats)
                return ResolveError::MissingColorAttachmentConfig;

            if(request.pipeline_data->color_blend)
            {
                const uint32_t blend_attachment_count = request.pipeline_data->color_blend->attachmentCount;
                if(blend_attachment_count > 0
                && blend_attachment_count != request.frame_output.color_attachment_count)
                    return ResolveError::FOColorBlendAttachmentMismatch;
            }

            return ResolveError::None;
        }

        void LogResolveStatsIfNeeded()
        {
            if((g_resolve_stats.requests % 64u) != 0u)
                return;

            GLogInfo("[PipelineResolver] req=%llu invalid=%llu incomplete_key=%llu fo_mismatch=%llu final_cache(h/m)=%llu/%llu materialize(ok/fail)=%llu/%llu",
                     (unsigned long long)g_resolve_stats.requests,
                     (unsigned long long)g_resolve_stats.invalid_request,
                     (unsigned long long)g_resolve_stats.incomplete_key,
                     (unsigned long long)g_resolve_stats.fo_mismatch,
                     (unsigned long long)g_resolve_stats.final_cache_hit,
                     (unsigned long long)g_resolve_stats.final_cache_miss,
                     (unsigned long long)g_resolve_stats.materialize_success,
                     (unsigned long long)g_resolve_stats.materialize_failed);
        }

        ValueArray<FinalPipelineCacheEntry> &GetFinalPipelineCache()
        {
            return g_monolithic_pipeline_cache;
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
        out_key.pipeline_config_hash = request.pipeline_config_hash;

        if(request.geometry_vertex_format)
        {
            out_key.vi.format_hash = HashGeometryVertexFormat(request.geometry_vertex_format);
            out_key.vi.attribute_count = request.geometry_vertex_format->GetCount();
            out_key.vi.binding_count = out_key.vi.attribute_count;
        }

        out_key.pr.shader_program_hash = HashShaderStages(request.shader_stages);
        out_key.pr.config_hash = HashPreRasterConfig(request.pipeline_data, request.primitive_type);
        if(request.pipeline_data)
            out_key.pr.primitive_type = request.primitive_type;

        out_key.fs.shader_program_hash = HashShaderStages(request.shader_stages);
        out_key.fs.variant_hash = 0;

        out_key.fo.color_attachment_count = request.frame_output.color_attachment_count;
        out_key.fo.color_formats_hash = (request.frame_output.color_formats && request.frame_output.color_attachment_count > 0)
                                      ? HashBytes(request.frame_output.color_formats, sizeof(VkFormat) * request.frame_output.color_attachment_count)
                                      : 0;
        out_key.fo.depth_stencil_format = request.frame_output.depth_stencil_format;
        out_key.fo.output_state_hash = HashFragmentOutputState(request.pipeline_data);
        if(request.pipeline_data && request.pipeline_data->multi_sample)
            out_key.fo.sample_count = static_cast<uint32_t>(request.pipeline_data->multi_sample->rasterizationSamples);

        return true;
    }

    bool PipelineResolver::HasCompleteFinalKey(const FinalPipelineKey &key)
    {
        if(key.pr.shader_program_hash == 0 || key.pr.config_hash == 0)
            return false;

        if(key.fs.shader_program_hash == 0)
            return false;

        if(key.fo.color_attachment_count == 0)
            return false;

        return true;
    }

    bool PipelineResolver::MaterializeMonolithic(const FinalPipelineResolveRequest &request, VkPipeline &out_pipeline)
    {
        out_pipeline = VK_NULL_HANDLE;

        if(!request.device || !request.pipeline_data || !request.shader_stages)
            return false;

        PipelineData *pd = request.pipeline_data;
        pd->InitShaderStage(*request.shader_stages);
        pd->InitVertexInputState();
        pd->SetColorAttachments(request.frame_output.color_attachment_count);
        pd->pipeline_info.layout = request.pipeline_layout;
        pd->pipeline_info.renderPass = VK_NULL_HANDLE;   // Dynamic Rendering
        pd->pipeline_info.subpass = request.subpass;

        // Dynamic Rendering：renderPass=VK_NULL_HANDLE 时 pNext 必须含
        // VkPipelineRenderingCreateInfo（VUID-VkGraphicsPipelineCreateInfo-renderPass-06061）
        // ——附件格式在此声明，替代传统 render pass 的 attachment 布局
        VkPipelineRenderingCreateInfo rendering_ci{};
        rendering_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_ci.pNext = pd->pipeline_info.pNext;
        rendering_ci.colorAttachmentCount = request.frame_output.color_attachment_count;
        rendering_ci.pColorAttachmentFormats = request.frame_output.color_formats;
        rendering_ci.depthAttachmentFormat = request.frame_output.depth_stencil_format;
        rendering_ci.stencilAttachmentFormat = request.frame_output.depth_stencil_format;

        pd->pipeline_info.pNext = &rendering_ci;

        return vkCreateGraphicsPipelines(*request.device,
                                         request.pipeline_cache,
                                         1,
                                         &pd->pipeline_info,
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
            if(request_error == ResolveError::FOColorBlendAttachmentMismatch)
                ++g_resolve_stats.fo_mismatch;

            GLogError("[PipelineResolver] Resolve failed: %s", ResolveErrorText(request_error));
            LogResolveStatsIfNeeded();
            return false;
        }

        const VulkanPhyDevice *physical_device = request.device ? request.device->GetPhyDevice() : nullptr;


        BuildFinalPipelineKey(request, out_result.key);
        if(!HasCompleteFinalKey(out_result.key))
        {
            ++g_resolve_stats.incomplete_key;
            GLogError("[PipelineResolver] Resolve failed: incomplete FinalPipelineKey");
            LogResolveStatsIfNeeded();
            return false;
        }

        bool vi_hit = false;
        bool pr_hit = false;
        bool fs_hit = false;
        bool fo_hit = false;

        if(TryGetCachedFinalPipeline(request, out_result.key, out_result.pipeline))
        {
            ++g_resolve_stats.final_cache_hit;
            LogResolveStatsIfNeeded();
            return true;
        }

        ++g_resolve_stats.final_cache_miss;

        if(!MaterializeMonolithic(request, out_result.pipeline))
        {
            ++g_resolve_stats.materialize_failed;
            GLogError("[PipelineResolver] Materialize failed in monolithic mode");
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

        auto ReleaseFromCache = [device, pipeline](ValueArray<FinalPipelineCacheEntry> &cache)
        {
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
        };

        ReleaseFromCache(g_monolithic_pipeline_cache);
    }

    void PipelineResolver::ClearCacheForDevice(VulkanDevice *device)
    {
        ThreadMutexLock resolver_lock(&g_resolver_mutex);

        if(!device)
            return;

        auto ClearFromCache = [device](ValueArray<FinalPipelineCacheEntry> &cache)
        {
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
        };

        ClearFromCache(g_monolithic_pipeline_cache);
        // Final executable pipelines have been released by their owners before
        // VulkanDevice destruction reaches this resolver cache. Destroy the
        // four library pipelines explicitly so they do not remain tracked as
        // live Vulkan objects.

        GLogInfo("[PipelineResolver] Cleared caches for device %p. Remaining: monolithic=%d",
                 (void *)device,
                 g_monolithic_pipeline_cache.GetCount());
    }


}//namespace hgl::graph
