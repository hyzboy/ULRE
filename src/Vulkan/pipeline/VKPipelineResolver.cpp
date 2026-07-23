#include<hgl/vk/pipeline/VKPipelineResolver.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/util/hash/FNV1a.h>
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
            if(pd->multi_sample)
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->multi_sample->rasterizationSamples);
            if(pd->depth_stencil)
                hash = hgl::hash::FNV1aAppendBytes(hash, pd->depth_stencil, sizeof(VkPipelineDepthStencilStateCreateInfo));
            if(pd->color_blend)
            {
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->color_blend->attachmentCount);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->color_blend->logicOpEnable);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, pd->color_blend->logicOp);
            }
            if(pd->color_blend_attachments && pd->color_blend)
                hash = hgl::hash::FNV1aAppendBytes(hash, pd->color_blend_attachments, sizeof(VkPipelineColorBlendAttachmentState) * pd->color_blend->attachmentCount);
            return hash;
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

        out_key.vi.format_hash = HashVIL(request.vertex_input_layout);
        if(request.vertex_input_layout)
        {
            out_key.vi.attribute_count = request.vertex_input_layout->GetVertexAttribCount();
            out_key.vi.binding_count = request.vertex_input_layout->GetVertexAttribCount();
        }

        out_key.pr.shader_program_hash = HashShaderStages(request.shader_stages);
        out_key.pr.config_hash = HashPreRasterConfig(request.pipeline_data);
        if(request.pipeline_data)
            out_key.pr.primitive_type = request.pipeline_data->input_assembly.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                                      ? PrimitiveType::Lines
                                      : PrimitiveType::Triangles;

        out_key.fs.shader_program_hash = HashShaderStages(request.shader_stages);
        out_key.fs.variant_hash = request.debug_name ? HashBytes(request.debug_name->c_str(), request.debug_name->Length()) : 0;

        out_key.fo.color_attachment_count = request.color_attachment_count;
        out_key.fo.color_formats_hash = (request.color_formats && request.color_attachment_count > 0)
                                      ? HashBytes(request.color_formats, sizeof(VkFormat) * request.color_attachment_count)
                                      : 0;
        out_key.fo.depth_stencil_format = request.depth_stencil_format;
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
        pd->SetColorAttachments(request.color_attachment_count);
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
        const VulkanPhyDevice *physical_device = request.device ? request.device->GetPhyDevice() : nullptr;
        out_result.capability = BuildCapabilityInfo(physical_device);
        out_result.materialize_mode = out_result.capability.preferred_materialize_mode;

        BuildFinalPipelineKey(request, out_result.key);
        if(!HasCompleteFinalKey(out_result.key))
            return false;

        switch(out_result.materialize_mode)
        {
        case PipelineMaterializeMode::GraphicsPipelineLibrary:
            // Phase 1 skeleton: route through the same monolithic materialization until GPL caches/linking land.
            return MaterializeMonolithic(request, out_result.pipeline);
        case PipelineMaterializeMode::Monolithic:
        default:
            return MaterializeMonolithic(request, out_result.pipeline);
        }
    }
}//namespace hgl::graph
