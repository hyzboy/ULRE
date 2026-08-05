#include<hgl/vk/pipeline/VKPipelineResolver.h>
#include<hgl/vk/VKPipelineConfig.h>
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
            hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->input_assembly.flags);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->input_assembly.topology);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->input_assembly.primitiveRestartEnable);
            if(pd->rasterization)
            {
                const VkPipelineRasterizationStateCreateInfo &rs = *pd->rasterization;
                hash = hgl::hash::FNV1aAppendValueBytes(hash, rs.flags);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, rs.depthClampEnable);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, rs.rasterizerDiscardEnable);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, rs.polygonMode);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, rs.cullMode);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, rs.frontFace);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, rs.depthBiasEnable);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, rs.depthBiasConstantFactor);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, rs.depthBiasClamp);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, rs.depthBiasSlopeFactor);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, rs.lineWidth);
            }
            if(pd->tessellation)
            {
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->tessellation->flags);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->tessellation->patchControlPoints);
            }

            hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->viewport_state.viewportCount);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->viewport_state.scissorCount);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->dynamic_state.dynamicStateCount);
            if(pd->dynamic_state.dynamicStateCount > 0 && pd->dynamic_state.pDynamicStates)
                hash = hgl::hash::FNV1aAppendBytes(hash,
                                                   pd->dynamic_state.pDynamicStates,
                                                   sizeof(VkDynamicState) * pd->dynamic_state.dynamicStateCount);
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
            VkRenderPass render_pass = VK_NULL_HANDLE;
            uint32_t subpass = 0;
            FinalPipelineKey key{};
            VkPipeline pipeline = VK_NULL_HANDLE;
            uint32_t references = 0;

            bool operator == (const FinalPipelineCacheEntry &rhs) const
            {
                return device == rhs.device
                    && pipeline_layout == rhs.pipeline_layout
                    && render_pass == rhs.render_pass
                    && subpass == rhs.subpass
                    && key == rhs.key
                    && pipeline == rhs.pipeline
                    && references == rhs.references;
            }
        };

        template<typename Key>
        struct LibraryPipelineCacheEntry
        {
            VulkanDevice *device = nullptr;
            VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
            VkRenderPass render_pass = VK_NULL_HANDLE;
            uint32_t subpass = 0;
            Key key{};
            VkPipeline pipeline = VK_NULL_HANDLE;

            bool operator == (const LibraryPipelineCacheEntry &rhs) const
            {
                return device == rhs.device
                    && pipeline_layout == rhs.pipeline_layout
                    && render_pass == rhs.render_pass
                    && subpass == rhs.subpass
                    && key == rhs.key
                    && pipeline == rhs.pipeline;
            }
        };

        ValueArray<FinalPipelineCacheEntry> g_monolithic_pipeline_cache;
        ValueArray<FinalPipelineCacheEntry> g_gpl_link_pipeline_cache;
        ValueArray<LibraryPipelineCacheEntry<VertexInterfaceKey>> g_vi_library_cache;
        ValueArray<LibraryPipelineCacheEntry<PreRasterPipelineKey>> g_pr_library_cache;
        ValueArray<LibraryPipelineCacheEntry<FragmentShaderKey>> g_fs_library_cache;
        ValueArray<LibraryPipelineCacheEntry<FragmentOutputKey>> g_fo_library_cache;

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
        ThreadMutex g_resolver_mutex;

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

        VkPipeline FindLibraryPipeline(const ValueArray<LibraryPipelineCacheEntry<VertexInterfaceKey>> &cache,
                                       const FinalPipelineResolveRequest &request,
                                       const VertexInterfaceKey &key)
        {
            const int count = cache.GetCount();
            for(int i = 0; i < count; ++i)
            {
                const LibraryPipelineCacheEntry<VertexInterfaceKey> &entry = cache[i];
                if(entry.device == request.device
                && entry.key == key)
                    return entry.pipeline;
            }

            return VK_NULL_HANDLE;
        }

        template<typename Key>
        VkPipeline FindLibraryPipeline(const ValueArray<LibraryPipelineCacheEntry<Key>> &cache,
                                       const FinalPipelineResolveRequest &request,
                                       const Key &key)
        {
            const int count = cache.GetCount();
            for(int i = 0; i < count; ++i)
            {
                const LibraryPipelineCacheEntry<Key> &entry = cache[i];
                if(entry.device == request.device
                && entry.pipeline_layout == request.pipeline_layout
                && entry.render_pass == request.render_pass
                && entry.subpass == request.subpass
                && entry.key == key)
                    return entry.pipeline;
            }

            return VK_NULL_HANDLE;
        }

        bool CacheLibraryPipeline(ValueArray<LibraryPipelineCacheEntry<VertexInterfaceKey>> &cache,
                                  const FinalPipelineResolveRequest &request,
                                  const VertexInterfaceKey &key,
                                  const VkPipeline pipeline)
        {
            if(!request.device || pipeline == VK_NULL_HANDLE)
                return false;

            LibraryPipelineCacheEntry<VertexInterfaceKey> entry{};
            entry.device = request.device;
            entry.pipeline_layout = VK_NULL_HANDLE;
            entry.render_pass = VK_NULL_HANDLE;
            entry.subpass = 0;
            entry.key = key;
            entry.pipeline = pipeline;
            return cache.Add(entry) >= 0;
        }

        template<typename Key>
        bool CacheLibraryPipeline(ValueArray<LibraryPipelineCacheEntry<Key>> &cache,
                                  const FinalPipelineResolveRequest &request,
                                  const Key &key,
                                  const VkPipeline pipeline)
        {
            if(!request.device || pipeline == VK_NULL_HANDLE)
                return false;

            LibraryPipelineCacheEntry<Key> entry{};
            entry.device = request.device;
            entry.pipeline_layout = request.pipeline_layout;
            entry.render_pass = request.render_pass;
            entry.subpass = request.subpass;
            entry.key = key;
            entry.pipeline = pipeline;
            return cache.Add(entry) >= 0;
        }

        template<typename Key>
        void ForgetLibraryPipelinesForDevice(ValueArray<LibraryPipelineCacheEntry<Key>> &cache,
                                             VulkanDevice *device)
        {
            int write = 0;
            const int count = cache.GetCount();
            for(int i = 0; i < count; ++i)
            {
                if(cache[i].device == device)
                    continue;

                if(write != i)
                    cache[write] = cache[i];
                ++write;
            }

            while(cache.GetCount() > write)
                cache.Delete(cache.GetCount() - 1);
        }

        template<typename Key>
        void DestroyLibraryPipelinesForDevice(ValueArray<LibraryPipelineCacheEntry<Key>> &cache,
                                              VulkanDevice *device)
        {
            for(int i = cache.GetCount() - 1; i >= 0; --i)
            {
                if(cache[i].device != device)
                    continue;

                if(cache[i].pipeline != VK_NULL_HANDLE)
                    vkDestroyPipeline(*device, cache[i].pipeline, nullptr);

                cache.Delete(i);
            }
        }

        template<typename Key>
        bool TouchLibraryCache(const ValueArray<LibraryPipelineCacheEntry<Key>> &cache,
                               const FinalPipelineResolveRequest &request,
                               const Key &key,
                               bool &hit)
        {
            hit = FindLibraryPipeline(cache, request, key) != VK_NULL_HANDLE;
            return hit;
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
                FinalPipelineCacheEntry &entry = cache[i];
                if(entry.device != request.device)
                    continue;
                if(entry.pipeline_layout != request.pipeline_layout)
                    continue;
                if(entry.render_pass != request.render_pass || entry.subpass != request.subpass)
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
            entry.render_pass = request.render_pass;
            entry.subpass = request.subpass;
            entry.key = key;
            entry.pipeline = pipeline;
            entry.references = 1;
            GetFinalPipelineCache(mode).Add(entry);
        }

        void CollectShaderStages(const ShaderStageCreateInfoList &source,
                                 const VkShaderStageFlagBits stage,
                                 const bool match,
                                 ShaderStageCreateInfoList &target)
        {
            const uint count = source.GetCount();
            const VkPipelineShaderStageCreateInfo *stages = source.GetData();

            for(uint i = 0; i < count; ++i)
                if(((stages[i].stage & stage) != 0) == match)
                    target.Add(stages[i]);
        }

        bool HasShaderStage(const ShaderStageCreateInfoList &stages,
                            const VkShaderStageFlagBits stage)
        {
            const uint count = stages.GetCount();
            const VkPipelineShaderStageCreateInfo *stage_list = stages.GetData();

            for(uint i = 0; i < count; ++i)
                if((stage_list[i].stage & stage) != 0)
                    return true;

            return false;
        }

        bool CreatePipelineLibrary(const FinalPipelineResolveRequest &request,
                                   PipelineData *pd,
                                   const ShaderStageCreateInfoList &stages,
                                   const VkGraphicsPipelineLibraryFlagsEXT library_flags,
                                   VkPipeline &out_pipeline)
        {
            out_pipeline = VK_NULL_HANDLE;

            VkGraphicsPipelineLibraryCreateInfoEXT library_info{};
            library_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
            library_info.flags = library_flags;

            const bool vertex_input_interface =
                (library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) != 0;
            const bool pre_rasterization =
                (library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) != 0;
            const bool fragment_shader =
                (library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) != 0;
            const bool fragment_output =
                (library_flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT) != 0;
            const bool has_tessellation_shader =
                HasShaderStage(stages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
                || HasShaderStage(stages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

            VkGraphicsPipelineCreateInfo create_info = pd->pipeline_info;
            create_info.pNext = &library_info;
            create_info.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
            create_info.layout = vertex_input_interface ? VK_NULL_HANDLE : request.pipeline_layout;
            create_info.renderPass = vertex_input_interface ? VK_NULL_HANDLE : request.render_pass;
            create_info.subpass = vertex_input_interface ? 0 : request.subpass;
            create_info.stageCount = stages.GetCount();
            create_info.pStages = stages.IsEmpty() ? nullptr : stages.GetData();

            create_info.pVertexInputState = vertex_input_interface ? pd->pipeline_info.pVertexInputState : nullptr;
            create_info.pInputAssemblyState = vertex_input_interface ? pd->pipeline_info.pInputAssemblyState : nullptr;
            create_info.pTessellationState = pre_rasterization && has_tessellation_shader
                                           ? pd->pipeline_info.pTessellationState
                                           : nullptr;
            create_info.pViewportState = pre_rasterization ? pd->pipeline_info.pViewportState : nullptr;
            create_info.pRasterizationState = pre_rasterization ? pd->pipeline_info.pRasterizationState : nullptr;
            create_info.pMultisampleState = (pre_rasterization || fragment_shader || fragment_output)
                                          ? pd->pipeline_info.pMultisampleState
                                          : nullptr;
            create_info.pDepthStencilState = fragment_shader ? pd->pipeline_info.pDepthStencilState : nullptr;
            create_info.pColorBlendState = fragment_output ? pd->pipeline_info.pColorBlendState : nullptr;

            // 按照规范精细化过滤各个 Library 的动态状态 (Dynamic States)
            VkPipelineDynamicStateCreateInfo lib_dynamic_state{};
            VkDynamicState filtered_dynamic_states[32];
            uint32_t filtered_count = 0;

            if(pd->pipeline_info.pDynamicState && pd->pipeline_info.pDynamicState->dynamicStateCount > 0)
            {
                const uint32_t src_count = pd->pipeline_info.pDynamicState->dynamicStateCount;
                const VkDynamicState *src_states = pd->pipeline_info.pDynamicState->pDynamicStates;

                for(uint32_t i = 0; i < src_count; ++i)
                {
                    const VkDynamicState ds = src_states[i];
                    bool belong = false;

                    if(vertex_input_interface)
                    {
                        if(ds == VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE
                        || ds == VK_DYNAMIC_STATE_VERTEX_INPUT_EXT)
                            belong = true;
                    }
                    if(pre_rasterization)
                    {
                        if(ds == VK_DYNAMIC_STATE_VIEWPORT
                        || ds == VK_DYNAMIC_STATE_SCISSOR
                        || ds == VK_DYNAMIC_STATE_LINE_WIDTH
                        || ds == VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE
                        || ds == VK_DYNAMIC_STATE_CULL_MODE
                        || ds == VK_DYNAMIC_STATE_FRONT_FACE
                        || ds == VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY
                        || ds == VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE)
                            belong = true;
                    }
                    if(fragment_shader)
                    {
                        if(ds == VK_DYNAMIC_STATE_DEPTH_BIAS
                        || ds == VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE
                        || ds == VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE
                        || ds == VK_DYNAMIC_STATE_DEPTH_COMPARE_OP
                        || ds == VK_DYNAMIC_STATE_DEPTH_BOUNDS
                        || ds == VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE
                        || ds == VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE
                        || ds == VK_DYNAMIC_STATE_STENCIL_OP
                        || ds == VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK
                        || ds == VK_DYNAMIC_STATE_STENCIL_WRITE_MASK
                        || ds == VK_DYNAMIC_STATE_STENCIL_REFERENCE)
                            belong = true;
                    }
                    if(fragment_output)
                    {
                        if(ds == VK_DYNAMIC_STATE_BLEND_CONSTANTS
                        || ds == VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT
                        || ds == VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT
                        || ds == VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT)
                            belong = true;
                    }

                    if(belong && filtered_count < 32)
                    {
                        filtered_dynamic_states[filtered_count++] = ds;
                    }
                }

                if(filtered_count > 0)
                {
                    lib_dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
                    lib_dynamic_state.pNext = nullptr;
                    lib_dynamic_state.flags = 0;
                    lib_dynamic_state.dynamicStateCount = filtered_count;
                    lib_dynamic_state.pDynamicStates = filtered_dynamic_states;
                    create_info.pDynamicState = &lib_dynamic_state;
                }
                else
                {
                    create_info.pDynamicState = nullptr;
                }
            }
            else
            {
                create_info.pDynamicState = nullptr;
            }

            const VkResult result = vkCreateGraphicsPipelines(*request.device,
                                                               request.pipeline_cache,
                                                               1,
                                                               &create_info,
                                                               nullptr,
                                                               &out_pipeline);
            if(result != VK_SUCCESS)
            {
                GLogError("[PipelineResolver] GPL library creation failed: flags=0x%x VkResult=%d",
                          static_cast<uint32_t>(library_flags),
                          static_cast<int>(result));
                out_pipeline = VK_NULL_HANDLE;
                return false;
            }

            return true;
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
        out_key.preset = request.pipeline_preset;

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

    bool PipelineResolver::MaterializeGraphicsPipelineLibrary(const FinalPipelineResolveRequest &request,
                                                             const FinalPipelineKey &key,
                                                             VkPipeline &out_pipeline)
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

        ShaderStageCreateInfoList pre_raster_stages;
        ShaderStageCreateInfoList fragment_stages;
        CollectShaderStages(*request.shader_stages, VK_SHADER_STAGE_FRAGMENT_BIT, false, pre_raster_stages);
        CollectShaderStages(*request.shader_stages, VK_SHADER_STAGE_FRAGMENT_BIT, true, fragment_stages);

        if(pre_raster_stages.IsEmpty() || fragment_stages.IsEmpty())
            return false;

        VkPipeline vi_pipeline = FindLibraryPipeline(g_vi_library_cache, request, key.vi);
        VkPipeline pr_pipeline = FindLibraryPipeline(g_pr_library_cache, request, key.pr);
        VkPipeline fs_pipeline = FindLibraryPipeline(g_fs_library_cache, request, key.fs);
        VkPipeline fo_pipeline = FindLibraryPipeline(g_fo_library_cache, request, key.fo);

        bool vi_created = false;
        bool pr_created = false;
        bool fs_created = false;
        bool fo_created = false;

        if(vi_pipeline == VK_NULL_HANDLE)
        {
            if(!CreatePipelineLibrary(request,
                                      pd,
                                      ShaderStageCreateInfoList{},
                                      VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
                                      vi_pipeline))
                goto fail;
            vi_created = true;
        }

        if(pr_pipeline == VK_NULL_HANDLE)
        {
            if(!CreatePipelineLibrary(request,
                                      pd,
                                      pre_raster_stages,
                                      VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
                                      pr_pipeline))
                goto fail;
            pr_created = true;
        }

        if(fs_pipeline == VK_NULL_HANDLE)
        {
            if(!CreatePipelineLibrary(request,
                                      pd,
                                      fragment_stages,
                                      VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
                                      fs_pipeline))
                goto fail;
            fs_created = true;
        }

        if(fo_pipeline == VK_NULL_HANDLE)
        {
            if(!CreatePipelineLibrary(request,
                                      pd,
                                      ShaderStageCreateInfoList{},
                                      VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
                                      fo_pipeline))
                goto fail;
            fo_created = true;
        }

        {
            VkPipeline libraries[] = {vi_pipeline, pr_pipeline, fs_pipeline, fo_pipeline};

            VkPipelineLibraryCreateInfoKHR library_link_info{};
            library_link_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
            library_link_info.libraryCount = 4;
            library_link_info.pLibraries = libraries;

            VkGraphicsPipelineCreateInfo link_info = pd->pipeline_info;
            link_info.pNext = &library_link_info;
            link_info.flags = 0;
            link_info.layout = request.pipeline_layout;
            link_info.renderPass = request.render_pass;
            link_info.subpass = request.subpass;
            link_info.stageCount = 0;
            link_info.pStages = nullptr;
            link_info.pVertexInputState = nullptr;
            link_info.pInputAssemblyState = nullptr;
            link_info.pTessellationState = nullptr;
            link_info.pViewportState = nullptr;
            link_info.pRasterizationState = nullptr;
            link_info.pMultisampleState = nullptr;
            link_info.pDepthStencilState = nullptr;
            link_info.pColorBlendState = nullptr;
            link_info.pDynamicState = nullptr;

            const VkResult link_result = vkCreateGraphicsPipelines(*request.device,
                                                                    request.pipeline_cache,
                                                                    1,
                                                                    &link_info,
                                                                    nullptr,
                                                                    &out_pipeline);
            if(link_result != VK_SUCCESS)
            {
                GLogError("[PipelineResolver] GPL link failed: VkResult=%d", static_cast<int>(link_result));
                out_pipeline = VK_NULL_HANDLE;
                goto fail;
            }
        }

        if(vi_created && !CacheLibraryPipeline(g_vi_library_cache, request, key.vi, vi_pipeline))
            goto fail;
        if(pr_created && !CacheLibraryPipeline(g_pr_library_cache, request, key.pr, pr_pipeline))
            goto fail;
        if(fs_created && !CacheLibraryPipeline(g_fs_library_cache, request, key.fs, fs_pipeline))
            goto fail;
        if(fo_created && !CacheLibraryPipeline(g_fo_library_cache, request, key.fo, fo_pipeline))
            goto fail;

        return true;

    fail:
        if(out_pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(*request.device, out_pipeline, nullptr);
            out_pipeline = VK_NULL_HANDLE;
        }
        if(vi_created && vi_pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(*request.device, vi_pipeline, nullptr);
        if(pr_created && pr_pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(*request.device, pr_pipeline, nullptr);
        if(fs_created && fs_pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(*request.device, fs_pipeline, nullptr);
        if(fo_created && fo_pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(*request.device, fo_pipeline, nullptr);
        return false;
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
        out_result.capability = BuildCapabilityInfo(physical_device);
        if(request.device && request.device->GetDevAttr())
            out_result.capability.graphics_pipeline_library =
                out_result.capability.graphics_pipeline_library
                && request.device->GetDevAttr()->graphics_pipeline_library;
        out_result.capability.preferred_materialize_mode =
            out_result.capability.graphics_pipeline_library
            ? PipelineMaterializeMode::GraphicsPipelineLibrary
            : PipelineMaterializeMode::Monolithic;
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
        TouchLibraryCache(g_vi_library_cache, request, out_result.key.vi, vi_hit);
        TouchLibraryCache(g_pr_library_cache, request, out_result.key.pr, pr_hit);
        TouchLibraryCache(g_fs_library_cache, request, out_result.key.fs, fs_hit);
        TouchLibraryCache(g_fo_library_cache, request, out_result.key.fo, fo_hit);
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
            if(!MaterializeGraphicsPipelineLibrary(request, out_result.key, out_result.pipeline))
            {
                GLogWarning("[PipelineResolver] GPL materialization failed, falling back to monolithic pipeline");
                out_result.materialize_mode = PipelineMaterializeMode::Monolithic;
                if(!MaterializeMonolithic(request, out_result.pipeline))
                {
                    ++g_resolve_stats.materialize_failed;
                    GLogError("[PipelineResolver] Materialize failed in GPL fallback");
                    LogResolveStatsIfNeeded();
                    return false;
                }
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
        ReleaseFromCache(g_gpl_link_pipeline_cache);
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
        ClearFromCache(g_gpl_link_pipeline_cache);
        // Final executable pipelines have been released by their owners before
        // VulkanDevice destruction reaches this resolver cache. Destroy the
        // four library pipelines explicitly so they do not remain tracked as
        // live Vulkan objects.
        DestroyLibraryPipelinesForDevice(g_vi_library_cache, device);
        DestroyLibraryPipelinesForDevice(g_pr_library_cache, device);
        DestroyLibraryPipelinesForDevice(g_fs_library_cache, device);
        DestroyLibraryPipelinesForDevice(g_fo_library_cache, device);

        GLogInfo("[PipelineResolver] Cleared caches for device %p. Remaining: monolithic=%d gpl=%d libraries vi/pr/fs/fo=%d/%d/%d/%d",
                 (void *)device,
                 g_monolithic_pipeline_cache.GetCount(),
                 g_gpl_link_pipeline_cache.GetCount(),
                 g_vi_library_cache.GetCount(),
                 g_pr_library_cache.GetCount(),
                 g_fs_library_cache.GetCount(),
                 g_fo_library_cache.GetCount());
    }

    PipelineResolverQueryStats PipelineResolver::QueryStats()
    {
        ThreadMutexLock resolver_lock(&g_resolver_mutex);

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
