#include<hgl/vk/pipeline/VKGraphicsPipelineLibraryBackend.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/type/ValueArray.h>
#include<hgl/log/Log.h>
#include<cstdint>

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

    GraphicsPipelineLibraryBackend &GraphicsPipelineLibraryBackend::Instance()
    {
        static GraphicsPipelineLibraryBackend backend;
        return backend;
    }

    GraphicsPipelineMode GraphicsPipelineLibraryBackend::GetMode() const
    {
        return GraphicsPipelineMode::GraphicsPipelineLibrary;
    }

    bool GraphicsPipelineLibraryBackend::Materialize(const FinalPipelineResolveRequest &request,
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

        const uint64_t pre_raster_hash = GraphicsPipelineBackend::HashShaderStages(request.device, &pre_raster_stages);
        const uint64_t fragment_hash = fragment_stages.IsEmpty()
                                     ? 0
                                     : GraphicsPipelineBackend::HashShaderStages(request.device, &fragment_stages);
        if(pre_raster_hash == 0 || (!fragment_stages.IsEmpty() && fragment_hash == 0))
        {
            GLogError("[PipelineResolver] GPL segment shader key is incomplete");
            return false;
        }

        AttachmentSignature attachments{};
        attachments.color_attachment_count = request.frame_output.color_attachment_count;
        attachments.color_formats_hash =
            request.frame_output.color_attachment_count > 0
                ? GraphicsPipelineBackend::HashBytes(request.frame_output.color_formats,
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
        VkPipeline fragment_shader_pipeline =
            FindLibraryPipeline(g_fragment_shader_library_cache,
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

        if(fragment_shader_pipeline == VK_NULL_HANDLE)
        {
            // The fragment-shader state subset is required for rasterizing
            // executables even when its shader-stage list is empty (depth-only).
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

    void GraphicsPipelineLibraryBackend::ClearCacheForDevice(VulkanDevice *device)
    {
        if(!device)
            return;

        DestroyLibraryPipelinesForDevice(g_pre_raster_library_cache, device);
        DestroyLibraryPipelinesForDevice(g_fragment_shader_library_cache, device);
        DestroyLibraryPipelinesForDevice(g_fragment_output_library_cache, device);
    }
}
