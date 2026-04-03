#include<hgl/vk/pipeline/VKGplLibraryHandleCache.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineBuildRequest.h>
#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/vk/pipeline/VKRenderFormat.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
namespace
{
// ─── Helpers ─────────────────────────────────────────────────────────────────

// All GPL library VkGraphicsPipelineCreateInfo share these two flags:
//   LIBRARY_BIT_KHR          — marks this as a library, not an executable pipeline
//   RETAIN_LINK_TIME_OPT_BIT — driver keeps intermediate IR for fast link
static constexpr VkPipelineCreateFlags kLibraryFlags =
    VK_PIPELINE_CREATE_LIBRARY_BIT_KHR |
    VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

// ─── VI Library ─────────────────────────────────────────────────────────────
// Captures: vertex binding/attribute descriptions + topology + primitive restart
// No shader stages, no layout.

VkPipeline CreateVILibrary(VkDevice device, VkPipelineCache cache,
                            const GraphicsPipelineBuildRequest &req)
{
    const VertexInputLayout *vil = req.vil;
    const PipelineData      *pd  = req.pipeline_data;
    const uint32_t           count = vil->GetVertexAttribCount();

    VkVertexInputBindingDescription   *bindings   = vil->NewBindListCopy();
    VkVertexInputAttributeDescription *attributes = vil->NewAttrListCopy();

    VkPipelineVertexInputStateCreateInfo vi_state{};
    vi_state.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi_state.vertexBindingDescriptionCount   = count;
    vi_state.pVertexBindingDescriptions      = bindings;
    vi_state.vertexAttributeDescriptionCount = count;
    vi_state.pVertexAttributeDescriptions    = attributes;

    VkPipelineInputAssemblyStateCreateInfo ia_state = pd->input_assembly;

    VkGraphicsPipelineLibraryCreateInfoEXT lib_info{};
    lib_info.sType  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
    lib_info.flags  = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

    VkGraphicsPipelineCreateInfo ci{};
    ci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ci.pNext               = &lib_info;
    ci.flags               = kLibraryFlags;
    ci.pVertexInputState   = &vi_state;
    ci.pInputAssemblyState = &ia_state;

    VkPipeline lib = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, cache, 1, &ci, nullptr, &lib) != VK_SUCCESS)
    {
        GLogError("[GplLibraryHandleCache] CreateVILibrary failed");
        lib = VK_NULL_HANDLE;
    }

    delete[] bindings;
    delete[] attributes;
    return lib;
}

// ─── PR Library ─────────────────────────────────────────────────────────────
// Captures: VS (+optional GS/TCS/TES) shader stage(s), rasterization state,
//           viewport state (dynamic), and depth format (no color).

VkPipeline CreatePRLibrary(VkDevice device, VkPipelineCache cache,
                            const GraphicsPipelineBuildRequest &req)
{
    const PipelineData *pd = req.pipeline_data;

    // Filter vertex-side shader stages from the material's combined list
    const ShaderStageCreateInfoList &all_stages = req.material->GetStageList();
    const uint32_t stage_count = static_cast<uint32_t>(all_stages.GetCount());

    // Collect pre-raster stages into a local array
    VkPipelineShaderStageCreateInfo pr_stages[16]; // VS + GS + TC + TE max 4, well below 16
    uint32_t pr_stage_count = 0;
    for (uint32_t i = 0; i < stage_count; ++i)
    {
        const VkShaderStageFlagBits stage = all_stages[i].stage;
        if (stage == VK_SHADER_STAGE_VERTEX_BIT
         || stage == VK_SHADER_STAGE_GEOMETRY_BIT
         || stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
         || stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        {
            pr_stages[pr_stage_count++] = all_stages[i];
        }
    }

    if (pr_stage_count == 0)
    {
        GLogError("[GplLibraryHandleCache] CreatePRLibrary: no vertex-side shader stages found");
        return VK_NULL_HANDLE;
    }

    // Dynamic viewport + scissor — pViewports/pScissors must be nullptr when dynamic
    VkPipelineViewportStateCreateInfo vp_state{};
    vp_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_state.viewportCount = 1;
    vp_state.scissorCount  = 1;
    vp_state.pViewports    = nullptr;
    vp_state.pScissors     = nullptr;

    // Depth format only — no color attachments at pre-raster stage
    VkPipelineRenderingCreateInfoKHR rendering_ci{};
    rendering_ci.sType                 = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_ci.colorAttachmentCount  = 0;
    rendering_ci.pColorAttachmentFormats = nullptr;
    rendering_ci.depthAttachmentFormat = req.render_format->GetDepthFormat();

    VkGraphicsPipelineLibraryCreateInfoEXT lib_info{};
    lib_info.sType  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
    lib_info.pNext  = &rendering_ci;
    lib_info.flags  = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

    VkGraphicsPipelineCreateInfo ci{};
    ci.sType              = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ci.pNext              = &lib_info;
    ci.flags              = kLibraryFlags;
    ci.stageCount         = pr_stage_count;
    ci.pStages            = pr_stages;
    ci.pViewportState     = &vp_state;
    ci.pRasterizationState= pd->rasterization;
    ci.layout             = req.material->GetPipelineLayout();

    // Dynamic states: viewport and scissor must be dynamic since VkPipelineViewportState has no data
    VkDynamicState dyn_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn_state_ci{};
    dyn_state_ci.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn_state_ci.dynamicStateCount = 2;
    dyn_state_ci.pDynamicStates    = dyn_states;
    ci.pDynamicState = &dyn_state_ci;

    VkPipeline lib = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, cache, 1, &ci, nullptr, &lib) != VK_SUCCESS)
    {
        GLogError("[GplLibraryHandleCache] CreatePRLibrary failed");
        return VK_NULL_HANDLE;
    }
    return lib;
}

// ─── FS Library ─────────────────────────────────────────────────────────────
// Captures: FS shader stage, depth/stencil state, multisample state, depth format.

VkPipeline CreateFSLibrary(VkDevice device, VkPipelineCache cache,
                            const GraphicsPipelineBuildRequest &req)
{
    const PipelineData *pd = req.pipeline_data;

    const ShaderStageCreateInfoList &all_stages = req.material->GetStageList();
    const uint32_t stage_count = static_cast<uint32_t>(all_stages.GetCount());

    VkPipelineShaderStageCreateInfo fs_stage{};
    bool found_fs = false;
    for (uint32_t i = 0; i < stage_count; ++i)
    {
        if (all_stages[i].stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            fs_stage  = all_stages[i];
            found_fs = true;
            break;
        }
    }

    if (!found_fs)
    {
        GLogError("[GplLibraryHandleCache] CreateFSLibrary: no fragment shader stage found");
        return VK_NULL_HANDLE;
    }

    VkPipelineRenderingCreateInfoKHR rendering_ci{};
    rendering_ci.sType                 = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_ci.colorAttachmentCount  = 0;
    rendering_ci.pColorAttachmentFormats = nullptr;
    rendering_ci.depthAttachmentFormat = req.render_format->GetDepthFormat();

    VkGraphicsPipelineLibraryCreateInfoEXT lib_info{};
    lib_info.sType  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
    lib_info.pNext  = &rendering_ci;
    lib_info.flags  = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

    VkGraphicsPipelineCreateInfo ci{};
    ci.sType             = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ci.pNext             = &lib_info;
    ci.flags             = kLibraryFlags;
    ci.stageCount        = 1;
    ci.pStages           = &fs_stage;
    ci.pDepthStencilState= pd->depth_stencil;
    ci.pMultisampleState = pd->multi_sample;
    ci.layout            = req.material->GetPipelineLayout();

    VkPipeline lib = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, cache, 1, &ci, nullptr, &lib) != VK_SUCCESS)
    {
        GLogError("[GplLibraryHandleCache] CreateFSLibrary failed");
        return VK_NULL_HANDLE;
    }
    return lib;
}

// ─── FO Library ─────────────────────────────────────────────────────────────
// Captures: color blend state (per-attachment), multisample state, color formats.
// No layout, no shader stages.

VkPipeline CreateFOLibrary(VkDevice device, VkPipelineCache cache,
                            const GraphicsPipelineBuildRequest &req)
{
    const PipelineData  *pd  = req.pipeline_data;
    const RenderFormat  *rf  = req.render_format;
    const uint32_t       n   = rf->GetColorCount();

    // Build per-attachment blend states: replicate [0] across all n color attachments
    // Vulkan spec guarantees color_blend_attachments is non-null when colorAttachmentCount > 0.
    VkPipelineColorBlendAttachmentState blend_attachments[16]; // max 8 color attachments, 16 is safe
    if (n > 16)
    {
        GLogError("[GplLibraryHandleCache] CreateFOLibrary: too many color attachments (%u)", n);
        return VK_NULL_HANDLE;
    }
    for (uint32_t i = 0; i < n; ++i)
        blend_attachments[i] = pd->color_blend_attachments[i < 1 ? 0 : i]; // use per-slot if available

    VkPipelineColorBlendStateCreateInfo blend_state{};
    blend_state.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.attachmentCount = n;
    blend_state.pAttachments    = blend_attachments;
    if (pd->color_blend)
    {
        blend_state.logicOpEnable = pd->color_blend->logicOpEnable;
        blend_state.logicOp       = pd->color_blend->logicOp;
        for (int k = 0; k < 4; ++k)
            blend_state.blendConstants[k] = pd->color_blend->blendConstants[k];
    }

    const VkFormat *color_fmts = rf->GetColorFormat().GetData();

    VkPipelineRenderingCreateInfoKHR rendering_ci{};
    rendering_ci.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_ci.colorAttachmentCount    = n;
    rendering_ci.pColorAttachmentFormats = color_fmts;

    VkGraphicsPipelineLibraryCreateInfoEXT lib_info{};
    lib_info.sType  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
    lib_info.pNext  = &rendering_ci;
    lib_info.flags  = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

    VkGraphicsPipelineCreateInfo ci{};
    ci.sType             = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ci.pNext             = &lib_info;
    ci.flags             = kLibraryFlags;
    ci.pMultisampleState = pd->multi_sample;
    ci.pColorBlendState  = &blend_state;

    VkPipeline lib = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, cache, 1, &ci, nullptr, &lib) != VK_SUCCESS)
    {
        GLogError("[GplLibraryHandleCache] CreateFOLibrary failed");
        return VK_NULL_HANDLE;
    }
    return lib;
}

// ─── Generic acquire helper ──────────────────────────────────────────────────
// Looks up key in map; on miss calls create_fn; inserts result only if non-null.
template<typename TKey, typename TCreateFn>
VkPipeline AcquireLibrary(std::unordered_map<TKey, VkPipeline> &map,
                           std::mutex &mtx,
                           const TKey &key,
                           TCreateFn create_fn)
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = map.find(key);
        if (it != map.end())
            return it->second;
    }

    // Create outside the lock — vkCreateGraphicsPipelines is thread-safe per Vulkan spec
    VkPipeline lib = create_fn();

    if (lib != VK_NULL_HANDLE)
    {
        std::lock_guard<std::mutex> lock(mtx);
        // Double-check: another thread may have created the same library concurrently
        auto [it, inserted] = map.emplace(key, lib);
        if (!inserted)
        {
            // Another thread won the race; discard ours
            // (lib we just created is abandoned; caller must not use it after this)
            // Return the winning handle instead
            return it->second;
        }
    }
    return lib;
}
}//anonymous namespace

// ─── GplLibraryHandleCache ─────────────────────────────────────────────────────────

GplLibraryHandleCache::~GplLibraryHandleCache()
{
    if (device_ == VK_NULL_HANDLE)
        return;

    std::lock_guard<std::mutex> lock(lib_mutex_);

    for (auto &[key, lib] : vi_lib_)
        vkDestroyPipeline(device_, lib, nullptr);
    for (auto &[key, lib] : pr_lib_)
        vkDestroyPipeline(device_, lib, nullptr);
    for (auto &[key, lib] : fs_lib_)
        vkDestroyPipeline(device_, lib, nullptr);
    for (auto &[key, lib] : fo_lib_)
        vkDestroyPipeline(device_, lib, nullptr);
}

void GplLibraryHandleCache::Init(VkDevice device, VkPipelineCache pipeline_cache)
{
    device_         = device;
    pipeline_cache_ = pipeline_cache;
}

VkPipeline GplLibraryHandleCache::AcquireVI(const GplVertexInputKey &key, const GraphicsPipelineBuildRequest &req)
{
    return AcquireLibrary(vi_lib_, lib_mutex_, key,
        [&]{ return CreateVILibrary(device_, pipeline_cache_, req); });
}

VkPipeline GplLibraryHandleCache::AcquirePR(const GplPreRasterKey &key, const GraphicsPipelineBuildRequest &req)
{
    return AcquireLibrary(pr_lib_, lib_mutex_, key,
        [&]{ return CreatePRLibrary(device_, pipeline_cache_, req); });
}

VkPipeline GplLibraryHandleCache::AcquireFS(const GplFragmentShaderKey &key, const GraphicsPipelineBuildRequest &req)
{
    return AcquireLibrary(fs_lib_, lib_mutex_, key,
        [&]{ return CreateFSLibrary(device_, pipeline_cache_, req); });
}

VkPipeline GplLibraryHandleCache::AcquireFO(const GplFragmentOutputKey &key, const GraphicsPipelineBuildRequest &req)
{
    return AcquireLibrary(fo_lib_, lib_mutex_, key,
        [&]{ return CreateFOLibrary(device_, pipeline_cache_, req); });
}
}//namespace hgl::graph
