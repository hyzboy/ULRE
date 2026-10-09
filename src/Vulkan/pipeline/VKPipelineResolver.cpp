#include<hgl/vk/pipeline/VKPipelineResolver.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/thread/ThreadMutex.h>
#include<hgl/util/hash/FNV1a.h>
#include<hgl/log/Log.h>
#include<cstring>

inline bool operator == (const VkPipelineColorBlendAttachmentState &lhs,
                         const VkPipelineColorBlendAttachmentState &rhs)
{
    return lhs.blendEnable == rhs.blendEnable
        && lhs.srcColorBlendFactor == rhs.srcColorBlendFactor
        && lhs.dstColorBlendFactor == rhs.dstColorBlendFactor
        && lhs.colorBlendOp == rhs.colorBlendOp
        && lhs.srcAlphaBlendFactor == rhs.srcAlphaBlendFactor
        && lhs.dstAlphaBlendFactor == rhs.dstAlphaBlendFactor
        && lhs.alphaBlendOp == rhs.alphaBlendOp
        && lhs.colorWriteMask == rhs.colorWriteMask;
}

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

        uint64_t HashShaderStages(VulkanDevice *device,
                                  const ShaderStageCreateInfoList *shader_stages,
                                  const VkShaderStageFlags stage_filter = 0)
        {
            if(!device || !shader_stages || shader_stages->IsEmpty())
                return 0;

            const uint count = shader_stages->GetCount();
            const VkPipelineShaderStageCreateInfo *stages = shader_stages->GetData();
            uint filtered_count = 0;

            for(uint i = 0; i < count; ++i)
                if(stage_filter == 0 || (stages[i].stage & stage_filter) != 0)
                    ++filtered_count;

            if(filtered_count == 0)
                return 0;

            hgl::hash::FNV1aHasher64 h;
            h << filtered_count;

            for(uint i = 0; i < count; ++i)
            {
                if(stage_filter != 0 && (stages[i].stage & stage_filter) == 0)
                    continue;

                // module 身份必须是 **SPIRV 内容 hash**，不能用 VkShaderModule 句柄值：
                // 句柄在 module 销毁后会被新建模块复用，两个不同的 shader 会算出同一个
                // key → 错误复用 pipeline（D2；与 resolvedRuntimePipelineMap 无 program
                // 键控同构）。句柄查不到内容 hash 时直接判键不完整（fail-fast：所有模块
                // 都经 VulkanDevice::CreateShaderModule 创建并登记）。
                const uint64_t module_hash = device->GetShaderModuleHash(stages[i].module);
                if(module_hash == 0)
                {
                    GLogError("[PipelineResolver] shader module 0x%llx (stage 0x%x) has no registered SPIRV hash"
                              " -- not created via VulkanDevice::CreateShaderModule? pipeline key cannot be built",
                              (unsigned long long)(uintptr_t)stages[i].module,
                              (unsigned)stages[i].stage);
                    return 0;
                }

                h << stages[i].stage
                  << stages[i].flags
                  << module_hash;
                if(stages[i].pName)
                    h << stages[i].pName;

                const VkSpecializationInfo *specialization = stages[i].pSpecializationInfo;
                h << static_cast<uint32_t>(specialization != nullptr);
                if(specialization)
                {
                    if((specialization->mapEntryCount > 0 && !specialization->pMapEntries)
                    || (specialization->dataSize > 0 && !specialization->pData))
                        return 0;

                    h << specialization->mapEntryCount;
                    for(uint32_t entry_index = 0; entry_index < specialization->mapEntryCount; ++entry_index)
                    {
                        const VkSpecializationMapEntry &entry = specialization->pMapEntries[entry_index];
                        h << entry.constantID << entry.offset << entry.size;
                    }

                    h << specialization->dataSize;
                    if(specialization->dataSize > 0)
                        h.AppendBytes(specialization->pData, specialization->dataSize);
                }
            }

            return h;
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

        bool HasOnlyMeshShaderStages(const ShaderStageCreateInfoList &stages)
        {
            bool has_mesh_shader = false;
            const uint count = stages.GetCount();
            const VkPipelineShaderStageCreateInfo *stage_list = stages.GetData();

            for(uint i = 0; i < count; ++i)
            {
                switch(stage_list[i].stage)
                {
                case VK_SHADER_STAGE_TASK_BIT_EXT:
                    break;
                case VK_SHADER_STAGE_MESH_BIT_EXT:
                    has_mesh_shader = true;
                    break;
                case VK_SHADER_STAGE_FRAGMENT_BIT:
                    break;
                default:
                    return false;
                }
            }

            return has_mesh_shader;
        }

        struct AttachmentSignature
        {
            uint64_t color_formats_hash = 0;
            VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;
            uint32_t color_attachment_count = 0;

            bool operator == (const AttachmentSignature &rhs) const
            {
                return color_formats_hash == rhs.color_formats_hash
                    && depth_stencil_format == rhs.depth_stencil_format
                    && color_attachment_count == rhs.color_attachment_count;
            }
        };

        struct PreRasterLibraryKey
        {
            uint64_t shader_stages_hash = 0;
            AttachmentSignature attachments{};

            bool operator == (const PreRasterLibraryKey &rhs) const
            {
                return shader_stages_hash == rhs.shader_stages_hash
                    && attachments == rhs.attachments;
            }
        };

        struct FragmentShaderLibraryKey
        {
            uint64_t shader_stages_hash = 0;
            AttachmentSignature attachments{};

            bool operator == (const FragmentShaderLibraryKey &rhs) const
            {
                return shader_stages_hash == rhs.shader_stages_hash
                    && attachments == rhs.attachments;
            }
        };

        struct FragmentOutputLibraryKey
        {
            AttachmentSignature attachments{};

            bool operator == (const FragmentOutputLibraryKey &rhs) const
            {
                return attachments == rhs.attachments;
            }
        };

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

        template<typename Key>
        struct LibraryPipelineCacheEntry
        {
            VulkanDevice *device = nullptr;
            VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
            Key key{};
            VkPipeline pipeline = VK_NULL_HANDLE;

            bool operator == (const LibraryPipelineCacheEntry &rhs) const
            {
                return device == rhs.device
                    && pipeline_layout == rhs.pipeline_layout
                    && key == rhs.key
                    && pipeline == rhs.pipeline;
            }
        };

        ValueArray<LibraryPipelineCacheEntry<PreRasterLibraryKey>> g_pre_raster_library_cache;
        ValueArray<LibraryPipelineCacheEntry<FragmentShaderLibraryKey>> g_fragment_shader_library_cache;
        ValueArray<LibraryPipelineCacheEntry<FragmentOutputLibraryKey>> g_fragment_output_library_cache;

        template<typename Key>
        VkPipeline FindLibraryPipeline(const ValueArray<LibraryPipelineCacheEntry<Key>> &cache,
                                       VulkanDevice *device,
                                       VkPipelineLayout pipeline_layout,
                                       const Key &key)
        {
            for(const LibraryPipelineCacheEntry<Key> &entry:cache)
                if(entry.device == device
                && entry.pipeline_layout == pipeline_layout
                && entry.key == key
                && entry.pipeline != VK_NULL_HANDLE)
                    return entry.pipeline;

            return VK_NULL_HANDLE;
        }

        template<typename Key>
        bool CacheLibraryPipeline(ValueArray<LibraryPipelineCacheEntry<Key>> &cache,
                                  VulkanDevice *device,
                                  VkPipelineLayout pipeline_layout,
                                  const Key &key,
                                  VkPipeline pipeline)
        {
            LibraryPipelineCacheEntry<Key> entry{};
            entry.device = device;
            entry.pipeline_layout = pipeline_layout;
            entry.key = key;
            entry.pipeline = pipeline;
            return cache.Add(entry) >= 0;
        }

        template<typename Key>
        void RemoveLibraryPipeline(ValueArray<LibraryPipelineCacheEntry<Key>> &cache,
                                   VulkanDevice *device,
                                   VkPipeline pipeline)
        {
            for(int i = cache.GetCount() - 1; i >= 0; --i)
                if(cache[i].device == device && cache[i].pipeline == pipeline)
                    cache.Delete(i);
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
            MissingColorAttachmentConfig,
            UnimplementedBackendMode,
            InvalidBackendMode
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
            case ResolveError::UnimplementedBackendMode: return "selected graphics pipeline backend is not implemented";
            case ResolveError::InvalidBackendMode: return "invalid graphics pipeline backend mode";
            default: return "none";
            }
        }

        const char *GraphicsPipelineModeName(const GraphicsPipelineMode mode)
        {
            switch(mode)
            {
            case GraphicsPipelineMode::Monolithic: return "Monolithic";
            case GraphicsPipelineMode::GraphicsPipelineLibrary: return "GraphicsPipelineLibrary";
            case GraphicsPipelineMode::ShaderObject: return "ShaderObject";
            default: return "Unknown";
            }
        }

        ResolveError ValidateResolveRequest(const FinalPipelineResolveRequest &request)
        {
            if(!request.device)
                return ResolveError::MissingDevice;

            const VulkanDevAttr *device_attr = request.device->GetDevAttr();
            if(!device_attr)
                return ResolveError::MissingDevice;

            switch(device_attr->graphics_pipeline_mode)
            {
            case GraphicsPipelineMode::Monolithic:
            case GraphicsPipelineMode::GraphicsPipelineLibrary:
                break;
            case GraphicsPipelineMode::ShaderObject:
                return ResolveError::UnimplementedBackendMode;
            default:
                return ResolveError::InvalidBackendMode;
            }

            if(!request.shader_stages || request.shader_stages->IsEmpty())
                return ResolveError::MissingShaderStages;

            if(request.pipeline_layout == VK_NULL_HANDLE)
                return ResolveError::MissingPipelineLayout;

            // depth-only 目标（如 shadow map）没有颜色附件，此时深度附件是唯一可写目标；
            // 两类附件都为空才是真正的配置缺失。
            if(request.frame_output.color_attachment_count == 0)
            {
                if(request.frame_output.depth_stencil_format == VK_FORMAT_UNDEFINED)
                    return ResolveError::MissingColorAttachmentConfig;
            }
            else
            if(!request.frame_output.color_formats)
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

        bool CreatePipelineLibrary(const FinalPipelineResolveRequest &request,
                                   const VkGraphicsPipelineLibraryFlagsEXT library_flags,
                                   const ShaderStageCreateInfoList &stages,
                                   const VkPipelineRenderingCreateInfo &rendering_info,
                                   const VkPipelineViewportStateCreateInfo *viewport_state,
                                   const VkPipelineRasterizationStateCreateInfo *rasterization_state,
                                   const VkPipelineMultisampleStateCreateInfo *multisample_state,
                                   const VkPipelineDepthStencilStateCreateInfo *depth_stencil_state,
                                   const VkPipelineColorBlendStateCreateInfo *color_blend_state,
                                   const VkPipelineDynamicStateCreateInfo *dynamic_state,
                                   VkPipeline &out_pipeline)
        {
            out_pipeline = VK_NULL_HANDLE;

            VkGraphicsPipelineLibraryCreateInfoEXT library_info{};
            library_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
            library_info.pNext = &rendering_info;
            library_info.flags = library_flags;

            VkGraphicsPipelineCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            create_info.pNext = &library_info;
            create_info.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
            if(request.device->IsDescriptorBufferActive())
                create_info.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
            create_info.stageCount = stages.GetCount();
            create_info.pStages = stages.IsEmpty() ? nullptr : stages.GetData();
            create_info.pViewportState = viewport_state;
            create_info.pRasterizationState = rasterization_state;
            create_info.pMultisampleState = multisample_state;
            create_info.pDepthStencilState = depth_stencil_state;
            create_info.pColorBlendState = color_blend_state;
            create_info.pDynamicState = dynamic_state;
            create_info.layout = request.pipeline_layout;
            create_info.renderPass = VK_NULL_HANDLE;
            create_info.subpass = 0;
            create_info.basePipelineIndex = -1;

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

    bool PipelineResolver::BuildFinalPipelineKey(const FinalPipelineResolveRequest &request, FinalPipelineKey &out_key)
    {
        out_key = {};

        if(request.device && request.device->GetDevAttr())
            out_key.backend_mode = request.device->GetDevAttr()->graphics_pipeline_mode;

        out_key.shader_stages_hash = HashShaderStages(request.device, request.shader_stages);

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

        // 零颜色附件合法，但仅限 depth-only——帧输出至少要有一个可写目标
        if(key.color_attachment_count == 0 && key.depth_stencil_format == VK_FORMAT_UNDEFINED)
            return false;

        return true;
    }

    bool PipelineResolver::MaterializePipeline(const FinalPipelineResolveRequest &request, VkPipeline &out_pipeline)
    {
        out_pipeline = VK_NULL_HANDLE;

        if(!request.device || !request.device->GetDevAttr())
            return false;

        switch(request.device->GetDevAttr()->graphics_pipeline_mode)
        {
        case GraphicsPipelineMode::Monolithic:
            return MaterializeMonolithicPipeline(request, out_pipeline);
        case GraphicsPipelineMode::GraphicsPipelineLibrary:
            return MaterializeGraphicsPipelineLibrary(request, out_pipeline);
        case GraphicsPipelineMode::ShaderObject:
            GLogError("[PipelineResolver] Graphics backend '%s' is not implemented",
                      GraphicsPipelineModeName(request.device->GetDevAttr()->graphics_pipeline_mode));
            return false;
        default:
            GLogError("[PipelineResolver] Invalid graphics backend mode value: %u",
                      static_cast<unsigned>(request.device->GetDevAttr()->graphics_pipeline_mode));
            return false;
        }
    }

    bool PipelineResolver::MaterializeMonolithicPipeline(const FinalPipelineResolveRequest &request, VkPipeline &out_pipeline)
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

    bool PipelineResolver::MaterializeGraphicsPipelineLibrary(const FinalPipelineResolveRequest &request,
                                                               VkPipeline &out_pipeline)
    {
        out_pipeline = VK_NULL_HANDLE;

        if(!request.device || !request.shader_stages)
            return false;

        if(!HasOnlyMeshShaderStages(*request.shader_stages))
        {
            GLogError("[PipelineResolver] GPL currently requires Task/Mesh/Fragment shader stages");
            return false;
        }

        ShaderStageCreateInfoList pre_raster_stages;
        ShaderStageCreateInfoList fragment_stages;
        CollectShaderStages(*request.shader_stages, VK_SHADER_STAGE_FRAGMENT_BIT, false, pre_raster_stages);
        CollectShaderStages(*request.shader_stages, VK_SHADER_STAGE_FRAGMENT_BIT, true, fragment_stages);

        const uint64_t pre_raster_hash = HashShaderStages(request.device, &pre_raster_stages);
        const uint64_t fragment_hash = fragment_stages.IsEmpty()
                                     ? 0
                                     : HashShaderStages(request.device, &fragment_stages);
        if(pre_raster_hash == 0 || (!fragment_stages.IsEmpty() && fragment_hash == 0))
        {
            GLogError("[PipelineResolver] GPL segment shader key is incomplete");
            return false;
        }

        AttachmentSignature attachments{};
        attachments.color_attachment_count = request.frame_output.color_attachment_count;
        attachments.color_formats_hash =
            request.frame_output.color_attachment_count > 0
                ? HashBytes(request.frame_output.color_formats,
                            sizeof(VkFormat) * request.frame_output.color_attachment_count)
                : 0;
        attachments.depth_stencil_format = request.frame_output.depth_stencil_format;

        PreRasterLibraryKey pre_raster_key{};
        pre_raster_key.shader_stages_hash = pre_raster_hash;
        pre_raster_key.attachments = attachments;

        FragmentShaderLibraryKey fragment_shader_key{};
        fragment_shader_key.shader_stages_hash = fragment_hash;
        fragment_shader_key.attachments = attachments;

        FragmentOutputLibraryKey fragment_output_key{};
        fragment_output_key.attachments = attachments;

        VkPipeline pre_raster_pipeline =
            FindLibraryPipeline(g_pre_raster_library_cache, request.device, request.pipeline_layout, pre_raster_key);
        VkPipeline fragment_shader_pipeline = fragment_stages.IsEmpty()
                                           ? VK_NULL_HANDLE
                                           : FindLibraryPipeline(g_fragment_shader_library_cache,
                                                                 request.device,
                                                                 request.pipeline_layout,
                                                                 fragment_shader_key);
        VkPipeline fragment_output_pipeline =
            FindLibraryPipeline(g_fragment_output_library_cache, request.device, request.pipeline_layout, fragment_output_key);

        bool pre_raster_created = false;
        bool fragment_shader_created = false;
        bool fragment_output_created = false;
        bool pre_raster_cached = false;
        bool fragment_shader_cached = false;
        bool fragment_output_cached = false;
        VkPipeline linked_pipeline = VK_NULL_HANDLE;

        auto cleanup = [&]()
        {
            if(linked_pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(*request.device, linked_pipeline, nullptr);

            if(pre_raster_cached)
                RemoveLibraryPipeline(g_pre_raster_library_cache, request.device, pre_raster_pipeline);
            if(fragment_shader_cached)
                RemoveLibraryPipeline(g_fragment_shader_library_cache, request.device, fragment_shader_pipeline);
            if(fragment_output_cached)
                RemoveLibraryPipeline(g_fragment_output_library_cache, request.device, fragment_output_pipeline);

            if(pre_raster_created && pre_raster_pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(*request.device, pre_raster_pipeline, nullptr);
            if(fragment_shader_created && fragment_shader_pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(*request.device, fragment_shader_pipeline, nullptr);
            if(fragment_output_created && fragment_output_pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(*request.device, fragment_output_pipeline, nullptr);
        };

        VkPipelineRenderingCreateInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_info.colorAttachmentCount = request.frame_output.color_attachment_count;
        rendering_info.pColorAttachmentFormats = request.frame_output.color_formats;
        rendering_info.depthAttachmentFormat = request.frame_output.depth_stencil_format;
        rendering_info.stencilAttachmentFormat = IsStencilFormat(request.frame_output.depth_stencil_format)
                                                   ? request.frame_output.depth_stencil_format
                                                   : VK_FORMAT_UNDEFINED;

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
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.depthBiasEnable = VK_FALSE;
        rasterization.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample.alphaToCoverageEnable = VK_FALSE;

        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_FALSE;
        depth_stencil.depthWriteEnable = VK_FALSE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
        depth_stencil.front.failOp = VK_STENCIL_OP_KEEP;
        depth_stencil.front.passOp = VK_STENCIL_OP_KEEP;
        depth_stencil.front.depthFailOp = VK_STENCIL_OP_KEEP;
        depth_stencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
        depth_stencil.back = depth_stencil.front;

        VkPipelineColorBlendAttachmentState blend_attachment{};
        blend_attachment.blendEnable = VK_FALSE;
        blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        ValueArray<VkPipelineColorBlendAttachmentState> blend_attachments;
        for(uint32_t i = 0; i < request.frame_output.color_attachment_count; ++i)
            if(blend_attachments.Add(blend_attachment) < 0)
            {
                GLogError("[PipelineResolver] Failed to allocate GPL color blend attachments");
                return false;
            }

        VkPipelineColorBlendStateCreateInfo color_blend{};
        color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend.attachmentCount = request.frame_output.color_attachment_count;
        color_blend.pAttachments = blend_attachments.IsEmpty() ? nullptr : blend_attachments.GetData();

        const VkDynamicState pre_raster_dynamic_states[] =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_CULL_MODE_EXT,
            VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
            VK_DYNAMIC_STATE_LINE_WIDTH
        };
        VkPipelineDynamicStateCreateInfo pre_raster_dynamic_state{};
        pre_raster_dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pre_raster_dynamic_state.dynamicStateCount =
            static_cast<uint32_t>(sizeof(pre_raster_dynamic_states) / sizeof(pre_raster_dynamic_states[0]));
        pre_raster_dynamic_state.pDynamicStates = pre_raster_dynamic_states;

        const VkDynamicState fragment_shader_dynamic_states[] =
        {
            VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,
            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
            VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT
        };
        VkPipelineDynamicStateCreateInfo fragment_shader_dynamic_state{};
        fragment_shader_dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        fragment_shader_dynamic_state.dynamicStateCount =
            static_cast<uint32_t>(sizeof(fragment_shader_dynamic_states) / sizeof(fragment_shader_dynamic_states[0]));
        fragment_shader_dynamic_state.pDynamicStates = fragment_shader_dynamic_states;

        const VkDynamicState fragment_output_dynamic_states[] =
        {
            VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,
            VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,
            VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,
            VK_DYNAMIC_STATE_BLEND_CONSTANTS
        };
        VkPipelineDynamicStateCreateInfo fragment_output_dynamic_state{};
        fragment_output_dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        fragment_output_dynamic_state.dynamicStateCount =
            static_cast<uint32_t>(sizeof(fragment_output_dynamic_states) / sizeof(fragment_output_dynamic_states[0]));
        fragment_output_dynamic_state.pDynamicStates = fragment_output_dynamic_states;

        ShaderStageCreateInfoList no_stages;
        if(pre_raster_pipeline == VK_NULL_HANDLE)
        {
            if(!CreatePipelineLibrary(request,
                                      VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
                                      pre_raster_stages,
                                      rendering_info,
                                      &viewport_state,
                                      &rasterization,
                                      nullptr,
                                      nullptr,
                                      nullptr,
                                      &pre_raster_dynamic_state,
                                      pre_raster_pipeline))
            {
                cleanup();
                return false;
            }
            pre_raster_created = true;
        }

        if(!fragment_stages.IsEmpty() && fragment_shader_pipeline == VK_NULL_HANDLE)
        {
            if(!CreatePipelineLibrary(request,
                                      VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
                                      fragment_stages,
                                      rendering_info,
                                      nullptr,
                                      nullptr,
                                      &multisample,
                                      &depth_stencil,
                                      nullptr,
                                      &fragment_shader_dynamic_state,
                                      fragment_shader_pipeline))
            {
                cleanup();
                return false;
            }
            fragment_shader_created = true;
        }

        if(fragment_output_pipeline == VK_NULL_HANDLE)
        {
            if(!CreatePipelineLibrary(request,
                                      VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
                                      no_stages,
                                      rendering_info,
                                      nullptr,
                                      nullptr,
                                      &multisample,
                                      nullptr,
                                      &color_blend,
                                      &fragment_output_dynamic_state,
                                      fragment_output_pipeline))
            {
                cleanup();
                return false;
            }
            fragment_output_created = true;
        }

        VkPipeline libraries[3]{};
        uint32_t library_count = 0;
        libraries[library_count++] = pre_raster_pipeline;
        if(fragment_shader_pipeline != VK_NULL_HANDLE)
            libraries[library_count++] = fragment_shader_pipeline;
        libraries[library_count++] = fragment_output_pipeline;

        VkPipelineLibraryCreateInfoKHR library_link_info{};
        library_link_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
        library_link_info.pNext = &rendering_info;
        library_link_info.libraryCount = library_count;
        library_link_info.pLibraries = libraries;

        VkGraphicsPipelineCreateInfo link_info{};
        link_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        link_info.pNext = &library_link_info;
        if(request.device->IsDescriptorBufferActive())
            link_info.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        link_info.layout = request.pipeline_layout;
        link_info.renderPass = VK_NULL_HANDLE;
        link_info.subpass = 0;
        link_info.basePipelineIndex = -1;
        if(fragment_stages.IsEmpty())
        {
            // A depth-only pipeline has no fragment-shader library. Supply its
            // depth state and dynamic depth state in the executable link info.
            link_info.pDepthStencilState = &depth_stencil;
            link_info.pDynamicState = &fragment_shader_dynamic_state;
        }

        const VkResult link_result = vkCreateGraphicsPipelines(*request.device,
                                                               request.pipeline_cache,
                                                               1,
                                                               &link_info,
                                                               nullptr,
                                                               &linked_pipeline);
        if(link_result != VK_SUCCESS)
        {
            GLogError("[PipelineResolver] GPL link failed: VkResult=%d", static_cast<int>(link_result));
            cleanup();
            return false;
        }

        if(pre_raster_created)
        {
            pre_raster_cached = CacheLibraryPipeline(g_pre_raster_library_cache,
                                                      request.device,
                                                      request.pipeline_layout,
                                                      pre_raster_key,
                                                      pre_raster_pipeline);
            if(!pre_raster_cached)
            {
                GLogError("[PipelineResolver] Failed to cache GPL pre-rasterization library");
                cleanup();
                return false;
            }
        }

        if(fragment_shader_created)
        {
            fragment_shader_cached = CacheLibraryPipeline(g_fragment_shader_library_cache,
                                                           request.device,
                                                           request.pipeline_layout,
                                                           fragment_shader_key,
                                                           fragment_shader_pipeline);
            if(!fragment_shader_cached)
            {
                GLogError("[PipelineResolver] Failed to cache GPL fragment-shader library");
                cleanup();
                return false;
            }
        }

        if(fragment_output_created)
        {
            fragment_output_cached = CacheLibraryPipeline(g_fragment_output_library_cache,
                                                           request.device,
                                                           request.pipeline_layout,
                                                           fragment_output_key,
                                                           fragment_output_pipeline);
            if(!fragment_output_cached)
            {
                GLogError("[PipelineResolver] Failed to cache GPL fragment-output library");
                cleanup();
                return false;
            }
        }

        out_pipeline = linked_pipeline;
        return true;
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

            if(request_error == ResolveError::UnimplementedBackendMode && request.device && request.device->GetDevAttr())
            {
                GLogError("[PipelineResolver] Resolve failed: %s ('%s')",
                          ResolveErrorText(request_error),
                          GraphicsPipelineModeName(request.device->GetDevAttr()->graphics_pipeline_mode));
            }
            else
            {
                GLogError("[PipelineResolver] Resolve failed: %s", ResolveErrorText(request_error));
            }
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

        DestroyLibraryPipelinesForDevice(g_pre_raster_library_cache, device);
        DestroyLibraryPipelinesForDevice(g_fragment_shader_library_cache, device);
        DestroyLibraryPipelinesForDevice(g_fragment_output_library_cache, device);

        GLogInfo("[PipelineResolver] Cleared caches for device %p. Remaining: %d",
                 (void *)device,
                 cache.GetCount());
    }


}//namespace hgl::graph
