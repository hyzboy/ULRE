#include<hgl/vk/pipeline/VKPipelineResolver.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/util/hash/FNV1a.h>
#include<hgl/log/Log.h>
#include<cstring>

namespace hgl::graph
{
    namespace
    {
        constexpr bool FORCE_DISABLE_GRAPHICS_PIPELINE_LIBRARY = true;

        uint64_t HashBytes(const void *data, const size_t size)
        {
            uint64_t hash = hgl::hash::FNV1aInit<uint64_t>();
            return hgl::hash::FNV1aAppendBytes(hash, data, size);
        }

        uint64_t HashVIL(const VIL *vil)
        {
            if(!vil)
                return 0;

            const uint32_t count = vil->GetVertexAttribCount();
            const VertexInputFormat *vif_list = vil->GetVIFList();
            if(!vif_list || count == 0)
                return 0;

            uint64_t hash = hgl::hash::FNV1aInit<uint64_t>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, count);
            hash = hgl::hash::FNV1aAppendBytes(hash, vif_list, sizeof(VertexInputFormat) * count);
            return hash;
        }

        uint64_t HashGeometryVertexFormat(const GeometryVertexFormat *gvf)
        {
            if(!gvf)
                return 0;

            const uint32_t count = gvf->GetCount();
            if(count == 0)
                return 0;

            uint64_t hash = hgl::hash::FNV1aInit<uint64_t>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, count);

            for(uint32_t i=0;i<count;i++)
            {
                const GeometryVertexAttributeFormat *attr = gvf->Get(i);
                if(!attr)
                    continue;

                hash = hgl::hash::FNV1aAppendValueBytes(hash, attr->semantic);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, attr->format);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, attr->vec_size);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, attr->stride);
            }

            return hash;
        }

        uint64_t HashShaderStages(const ShaderStageCreateInfoList *shader_stages)
        {
            if(!shader_stages || shader_stages->IsEmpty())
                return 0;

            uint64_t hash = hgl::hash::FNV1aInit<uint64_t>();
            const uint count = shader_stages->GetCount();
            const VkPipelineShaderStageCreateInfo *stages = shader_stages->GetData();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, count);

            for(uint i = 0; i < count; ++i)
            {
                hash = hgl::hash::FNV1aAppendValueBytes(hash, stages[i].stage);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, (uint64_t)(uintptr_t)stages[i].module);
                if(stages[i].pName)
                    hash = hgl::hash::FNV1aAppendBytes(hash, stages[i].pName, std::strlen(stages[i].pName));
            }

            return hash;
        }

        uint64_t HashPreRasterConfig(const PipelineData *pd)
        {
            if(!pd)
                return 0;

            uint64_t hash = hgl::hash::FNV1aInit<uint64_t>();
            hash = hgl::hash::FNV1aAppendBytes(hash, &pd->input_assembly, sizeof(pd->input_assembly));
            if(pd->rasterization)
                hash = hgl::hash::FNV1aAppendBytes(hash, pd->rasterization, sizeof(VkPipelineRasterizationStateCreateInfo));
            if(pd->tessellation)
                hash = hgl::hash::FNV1aAppendBytes(hash, pd->tessellation, sizeof(VkPipelineTessellationStateCreateInfo));
            hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->dynamic_state.dynamicStateCount);
            return hash;
        }

        uint64_t HashFragmentOutputState(const PipelineData *pd)
        {
            if(!pd)
                return 0;

            uint64_t hash = hgl::hash::FNV1aInit<uint64_t>();

            const bool has_multisample = pd->multi_sample != nullptr;
            hash = hgl::hash::FNV1aAppendValueBytes(hash, has_multisample);
            if(has_multisample)
            {
                const VkPipelineMultisampleStateCreateInfo *ms = pd->multi_sample;
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ms->flags);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ms->rasterizationSamples);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ms->sampleShadingEnable);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ms->minSampleShading);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ms->alphaToCoverageEnable);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ms->alphaToOneEnable);

                const uint32_t sample_count = static_cast<uint32_t>(ms->rasterizationSamples);
                const uint32_t sample_mask_word_count = (sample_count + 31u) / 32u;
                hash = hgl::hash::FNV1aAppendValueBytes(hash, sample_mask_word_count);

                if(sample_mask_word_count > 0 && ms->pSampleMask)
                    hash = hgl::hash::FNV1aAppendBytes(hash, ms->pSampleMask, sizeof(VkSampleMask) * sample_mask_word_count);
            }

            const bool has_depth_stencil = pd->depth_stencil != nullptr;
            hash = hgl::hash::FNV1aAppendValueBytes(hash, has_depth_stencil);
            if(has_depth_stencil)
            {
                const VkPipelineDepthStencilStateCreateInfo *ds = pd->depth_stencil;
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->flags);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->depthTestEnable);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->depthWriteEnable);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->depthCompareOp);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->depthBoundsTestEnable);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->stencilTestEnable);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->front.failOp);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->front.passOp);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->front.depthFailOp);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->front.compareOp);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->front.compareMask);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->front.writeMask);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->front.reference);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->back.failOp);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->back.passOp);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->back.depthFailOp);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->back.compareOp);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->back.compareMask);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->back.writeMask);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->back.reference);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->minDepthBounds);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ds->maxDepthBounds);
            }

            const bool has_color_blend = pd->color_blend != nullptr;
            hash = hgl::hash::FNV1aAppendValueBytes(hash, has_color_blend);
            if(pd->color_blend)
            {
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->color_blend->attachmentCount);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->color_blend->flags);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->color_blend->logicOpEnable);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->color_blend->logicOp);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->color_blend->blendConstants[0]);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->color_blend->blendConstants[1]);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->color_blend->blendConstants[2]);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->color_blend->blendConstants[3]);
            }
            if(pd->color_blend_attachments && pd->color_blend)
                hash = hgl::hash::FNV1aAppendBytes(hash, pd->color_blend_attachments, sizeof(VkPipelineColorBlendAttachmentState) * pd->color_blend->attachmentCount);
            return hash;
        }

        struct FinalPipelineCacheEntry
        {
            VulkanDevice *device = nullptr;
            VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
            FinalPipelineKey key{};
            VkPipeline pipeline = VK_NULL_HANDLE;

            bool operator == (const FinalPipelineCacheEntry &rhs) const
            {
                return device == rhs.device
                    && pipeline_layout == rhs.pipeline_layout
                    && key == rhs.key
                    && pipeline == rhs.pipeline;
            }
        };

        ValueArray<FinalPipelineCacheEntry> g_monolithic_pipeline_cache;
        ValueArray<FinalPipelineCacheEntry> g_gpl_link_pipeline_cache;
        ValueArray<VertexInterfaceKey> g_vi_library_cache;
        ValueArray<PreRasterPipelineKey> g_pr_library_cache;
        ValueArray<FragmentShaderKey> g_fs_library_cache;
        ValueArray<FragmentOutputKey> g_fo_library_cache;

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
            uint64_t vi_library_hit = 0;
            uint64_t vi_library_miss = 0;
            uint64_t pr_library_hit = 0;
            uint64_t pr_library_miss = 0;
            uint64_t fs_library_hit = 0;
            uint64_t fs_library_miss = 0;
            uint64_t fo_library_hit = 0;
            uint64_t fo_library_miss = 0;
        };

        enum class ResolveError:uint8
        {
            None = 0,
            MissingDevice,
            MissingPipelineData,
            MissingShaderStages,
            MissingPipelineLayout,
            MissingRenderPass,
            MissingColorAttachmentConfig,
            FOColorBlendAttachmentMismatch
        };

        PipelineResolveStats g_resolve_stats;

        const char *ResolveErrorText(const ResolveError error)
        {
            switch(error)
            {
            case ResolveError::MissingDevice: return "missing device";
            case ResolveError::MissingPipelineData: return "missing pipeline_data";
            case ResolveError::MissingShaderStages: return "missing shader_stages";
            case ResolveError::MissingPipelineLayout: return "missing pipeline_layout";
            case ResolveError::MissingRenderPass: return "missing render_pass";
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

            if(request.render_pass == VK_NULL_HANDLE)
                return ResolveError::MissingRenderPass;

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

        bool TouchVILibraryCache(const VertexInterfaceKey &key, bool &hit)
        {
            const int count = g_vi_library_cache.GetCount();
            for(int i = 0; i < count; ++i)
            {
                if(g_vi_library_cache[i] == key)
                {
                    hit = true;
                    return true;
                }
            }

            hit = false;
            return g_vi_library_cache.Add(key);
        }

        bool TouchPRLibraryCache(const PreRasterPipelineKey &key, bool &hit)
        {
            const int count = g_pr_library_cache.GetCount();
            for(int i = 0; i < count; ++i)
            {
                if(g_pr_library_cache[i] == key)
                {
                    hit = true;
                    return true;
                }
            }

            hit = false;
            return g_pr_library_cache.Add(key);
        }

        bool TouchFSLibraryCache(const FragmentShaderKey &key, bool &hit)
        {
            const int count = g_fs_library_cache.GetCount();
            for(int i = 0; i < count; ++i)
            {
                if(g_fs_library_cache[i] == key)
                {
                    hit = true;
                    return true;
                }
            }

            hit = false;
            return g_fs_library_cache.Add(key);
        }

        bool TouchFOLibraryCache(const FragmentOutputKey &key, bool &hit)
        {
            const int count = g_fo_library_cache.GetCount();
            for(int i = 0; i < count; ++i)
            {
                if(g_fo_library_cache[i] == key)
                {
                    hit = true;
                    return true;
                }
            }

            hit = false;
            return g_fo_library_cache.Add(key);
        }

        void UpdateLibraryHitStats(const bool vi_hit,
                                   const bool pr_hit,
                                   const bool fs_hit,
                                   const bool fo_hit)
        {
            if(vi_hit) ++g_resolve_stats.vi_library_hit; else ++g_resolve_stats.vi_library_miss;
            if(pr_hit) ++g_resolve_stats.pr_library_hit; else ++g_resolve_stats.pr_library_miss;
            if(fs_hit) ++g_resolve_stats.fs_library_hit; else ++g_resolve_stats.fs_library_miss;
            if(fo_hit) ++g_resolve_stats.fo_library_hit; else ++g_resolve_stats.fo_library_miss;
        }

        void LogResolveStatsIfNeeded()
        {
            if((g_resolve_stats.requests % 64u) != 0u)
                return;

            GLogInfo("[PipelineResolver] req=%llu invalid=%llu incomplete_key=%llu fo_mismatch=%llu final_cache(h/m)=%llu/%llu materialize(ok/fail)=%llu/%llu segment_hits vi/pr/fs/fo=%llu/%llu/%llu/%llu",
                     (unsigned long long)g_resolve_stats.requests,
                     (unsigned long long)g_resolve_stats.invalid_request,
                     (unsigned long long)g_resolve_stats.incomplete_key,
                     (unsigned long long)g_resolve_stats.fo_mismatch,
                     (unsigned long long)g_resolve_stats.final_cache_hit,
                     (unsigned long long)g_resolve_stats.final_cache_miss,
                     (unsigned long long)g_resolve_stats.materialize_success,
                     (unsigned long long)g_resolve_stats.materialize_failed,
                     (unsigned long long)g_resolve_stats.vi_library_hit,
                     (unsigned long long)g_resolve_stats.pr_library_hit,
                     (unsigned long long)g_resolve_stats.fs_library_hit,
                     (unsigned long long)g_resolve_stats.fo_library_hit);
        }

        ValueArray<FinalPipelineCacheEntry> &GetFinalPipelineCache(const PipelineMaterializeMode mode)
        {
            return mode == PipelineMaterializeMode::GraphicsPipelineLibrary
                 ? g_gpl_link_pipeline_cache
                 : g_monolithic_pipeline_cache;
        }

        bool TryGetCachedFinalPipeline(const PipelineMaterializeMode mode,
                                       const FinalPipelineResolveRequest &request,
                                       const FinalPipelineKey &key,
                                       VkPipeline &out_pipeline)
        {
            if(!request.device || request.pipeline_layout == VK_NULL_HANDLE)
                return false;

            ValueArray<FinalPipelineCacheEntry> &cache = GetFinalPipelineCache(mode);

            const int count = cache.GetCount();
            for(int i = 0; i < count; ++i)
            {
                const FinalPipelineCacheEntry &entry = cache[i];
                if(entry.device != request.device)
                    continue;
                if(entry.pipeline_layout != request.pipeline_layout)
                    continue;
                if(!(entry.key == key))
                    continue;
                if(entry.pipeline == VK_NULL_HANDLE)
                    continue;

                out_pipeline = entry.pipeline;
                return true;
            }

            return false;
        }

        void CacheFinalPipeline(const PipelineMaterializeMode mode,
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
            GetFinalPipelineCache(mode).Add(entry);
        }
    }

    PipelineCapabilityInfo PipelineResolver::BuildCapabilityInfo(const VulkanPhyDevice *physical_device)
    {
        PipelineCapabilityInfo info{};
        if(!physical_device)
            return info;

        info.graphics_pipeline_library = !FORCE_DISABLE_GRAPHICS_PIPELINE_LIBRARY
                                      && physical_device->SupportGraphicsPipelineLibrary();
        info.preferred_materialize_mode = info.graphics_pipeline_library
                                        ? PipelineMaterializeMode::GraphicsPipelineLibrary
                                        : PipelineMaterializeMode::Monolithic;
        return info;
    }

    PipelineMaterializeMode PipelineResolver::ResolveMaterializeMode(const VulkanPhyDevice *physical_device)
    {
        return BuildCapabilityInfo(physical_device).preferred_materialize_mode;
    }

    bool PipelineResolver::BuildFinalPipelineKey(const FinalPipelineResolveRequest &request, FinalPipelineKey &out_key)
    {
        out_key = {};

        if(request.geometry_vertex_format)
        {
            out_key.vi.format_hash = HashGeometryVertexFormat(request.geometry_vertex_format);
            out_key.vi.attribute_count = request.geometry_vertex_format->GetCount();

            // Phase 2 transition: preserve uniqueness against VIL-era binding layouts.
            if(request.vertex_input_layout)
            {
                out_key.vi.format_hash = hgl::hash::FNV1aAppendValueBytes(out_key.vi.format_hash, HashVIL(request.vertex_input_layout));
                out_key.vi.binding_count = request.vertex_input_layout->GetVertexAttribCount();
            }
            else
            {
                out_key.vi.binding_count = out_key.vi.attribute_count;
            }
        }
        else
        {
            out_key.vi.format_hash = HashVIL(request.vertex_input_layout);
            if(request.vertex_input_layout)
            {
                out_key.vi.attribute_count = request.vertex_input_layout->GetVertexAttribCount();
                out_key.vi.binding_count = request.vertex_input_layout->GetVertexAttribCount();
            }
        }

        out_key.pr.shader_program_hash = HashShaderStages(request.shader_stages);
        out_key.pr.config_hash = HashPreRasterConfig(request.pipeline_data);
        if(request.pipeline_data)
            out_key.pr.primitive_type = request.pipeline_data->input_assembly.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                                      ? PrimitiveType::Lines
                                      : PrimitiveType::Triangles;

        out_key.fs.shader_program_hash = HashShaderStages(request.shader_stages);
        out_key.fs.variant_hash = request.debug_name ? HashBytes(request.debug_name->c_str(), request.debug_name->Length()) : 0;

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
        pd->InitVertexInputState(request.vertex_input_layout);
        pd->SetColorAttachments(request.frame_output.color_attachment_count);
        pd->pipeline_info.layout = request.pipeline_layout;
        pd->pipeline_info.renderPass = request.render_pass;
        pd->pipeline_info.subpass = request.subpass;

        return vkCreateGraphicsPipelines(*request.device,
                                         request.pipeline_cache,
                                         1,
                                         &pd->pipeline_info,
                                         nullptr,
                                         &out_pipeline) == VK_SUCCESS;
    }

    bool PipelineResolver::ResolveFinalPipeline(const FinalPipelineResolveRequest &request, FinalPipelineResolveResult &out_result)
    {
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
        out_result.capability = BuildCapabilityInfo(physical_device);
        out_result.materialize_mode = out_result.capability.preferred_materialize_mode;

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
        TouchVILibraryCache(out_result.key.vi, vi_hit);
        TouchPRLibraryCache(out_result.key.pr, pr_hit);
        TouchFSLibraryCache(out_result.key.fs, fs_hit);
        TouchFOLibraryCache(out_result.key.fo, fo_hit);
        UpdateLibraryHitStats(vi_hit, pr_hit, fs_hit, fo_hit);

        if(TryGetCachedFinalPipeline(out_result.materialize_mode, request, out_result.key, out_result.pipeline))
        {
            ++g_resolve_stats.final_cache_hit;
            LogResolveStatsIfNeeded();
            return true;
        }

        ++g_resolve_stats.final_cache_miss;

        switch(out_result.materialize_mode)
        {
        case PipelineMaterializeMode::GraphicsPipelineLibrary:
            // Phase 1 skeleton: route through the same monolithic materialization until GPL caches/linking land.
            if(!MaterializeMonolithic(request, out_result.pipeline))
            {
                ++g_resolve_stats.materialize_failed;
                GLogError("[PipelineResolver] Materialize failed in GPL mode fallback");
                LogResolveStatsIfNeeded();
                return false;
            }
            ++g_resolve_stats.materialize_success;
            CacheFinalPipeline(out_result.materialize_mode, request, out_result.key, out_result.pipeline);
            LogResolveStatsIfNeeded();
            return true;
        case PipelineMaterializeMode::Monolithic:
        default:
            if(!MaterializeMonolithic(request, out_result.pipeline))
            {
                ++g_resolve_stats.materialize_failed;
                GLogError("[PipelineResolver] Materialize failed in monolithic mode");
                LogResolveStatsIfNeeded();
                return false;
            }
            ++g_resolve_stats.materialize_success;
            CacheFinalPipeline(out_result.materialize_mode, request, out_result.key, out_result.pipeline);
            LogResolveStatsIfNeeded();
            return true;
        }
    }

    void PipelineResolver::ClearCacheForDevice(VulkanDevice *device)
    {
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
        ClearFromCache(g_gpl_link_pipeline_cache);

        GLogInfo("[PipelineResolver] Cleared caches for device %p. Remaining: monolithic=%d gpl=%d",
                 (void *)device,
                 g_monolithic_pipeline_cache.GetCount(),
                 g_gpl_link_pipeline_cache.GetCount());
    }

    PipelineResolverQueryStats PipelineResolver::QueryStats()
    {
        PipelineResolverQueryStats out{};
        out.requests               = g_resolve_stats.requests;
        out.invalid_request        = g_resolve_stats.invalid_request;
        out.incomplete_key         = g_resolve_stats.incomplete_key;
        out.fo_mismatch            = g_resolve_stats.fo_mismatch;
        out.final_cache_hit        = g_resolve_stats.final_cache_hit;
        out.final_cache_miss       = g_resolve_stats.final_cache_miss;
        out.materialize_success    = g_resolve_stats.materialize_success;
        out.materialize_failed     = g_resolve_stats.materialize_failed;
        out.vi_library_hit         = g_resolve_stats.vi_library_hit;
        out.vi_library_miss        = g_resolve_stats.vi_library_miss;
        out.pr_library_hit         = g_resolve_stats.pr_library_hit;
        out.pr_library_miss        = g_resolve_stats.pr_library_miss;
        out.fs_library_hit         = g_resolve_stats.fs_library_hit;
        out.fs_library_miss        = g_resolve_stats.fs_library_miss;
        out.fo_library_hit         = g_resolve_stats.fo_library_hit;
        out.fo_library_miss        = g_resolve_stats.fo_library_miss;
        out.monolithic_cache_entries = static_cast<uint32_t>(g_monolithic_pipeline_cache.GetCount());
        out.gpl_cache_entries        = static_cast<uint32_t>(g_gpl_link_pipeline_cache.GetCount());
        out.vi_library_entries       = static_cast<uint32_t>(g_vi_library_cache.GetCount());
        out.pr_library_entries       = static_cast<uint32_t>(g_pr_library_cache.GetCount());
        out.fs_library_entries       = static_cast<uint32_t>(g_fs_library_cache.GetCount());
        out.fo_library_entries       = static_cast<uint32_t>(g_fo_library_cache.GetCount());
        return out;
    }
}//namespace hgl::graph
