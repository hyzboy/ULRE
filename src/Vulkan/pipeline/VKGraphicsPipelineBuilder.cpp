#include<hgl/vk/pipeline/VKGraphicsPipelineBuilder.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/pipeline/VKGplLibraryHandleCache.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineBuildRequest.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/vk/pipeline/VKRenderTargetFormat.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

namespace
{
static bool IsPullingMaterial(const ShaderMaterialProgram *material)
{
    if (!material)
        return false;

    return material->IsPullingEnabled()
        || material->hasSet(DescriptorSetType::VertexStreams);
}

static void LogVertexInputLayoutDetails(const char *tag, const VIL *vil)
{
    if (!vil)
    {
        GLogInfo("[%s] VIL=null", tag ? tag : "VertexInputDiag");
        return;
    }

    const uint32_t count = vil->GetVertexAttribCount();
    GLogInfo("[%s] VIL attr_count=%u", tag ? tag : "VertexInputDiag", count);

    for (uint32_t i = 0; i < count; ++i)
    {
        const VertexInputFormat *cfg = vil->GetConfig(i);
        if (!cfg)
            continue;

        const char *fmt_name = GetVulkanFormatName(cfg->format);
        GLogInfo("[%s]   [%u] attrib=%u binding=%d format=%d(%s) vec_size=%u stride=%u input_rate=%u",
                 tag ? tag : "VertexInputDiag",
                 i,
                 static_cast<unsigned>(cfg->attrib),
                 cfg->binding,
                 static_cast<int>(cfg->format),
                 fmt_name ? fmt_name : "<unknown>",
                 cfg->vec_size,
                 cfg->stride,
                 static_cast<unsigned>(cfg->input_rate));
    }
}

static void LogPipelineVertexInputComparison(const GraphicsPipelineBuildRequest &request)
{
    if (!request.material)
        return;

    if (IsPullingMaterial(request.material))
    {
        LogVertexInputLayoutDetails("PipelineBuild.GeometryVIL", request.vil);
        GLogInfo("[PipelineBuild.MaterialDefaultVIL] Pulling enabled: forcing empty VkPipelineVertexInputState");
        return;
    }

    const VIL *material_vil = request.material->GetDefaultVIL();
    LogVertexInputLayoutDetails("PipelineBuild.GeometryVIL", request.vil);
    LogVertexInputLayoutDetails("PipelineBuild.MaterialDefaultVIL", material_vil);

    if (!request.vil || !material_vil)
        return;

    const uint32_t g_count = request.vil->GetVertexAttribCount();
    const uint32_t m_count = material_vil->GetVertexAttribCount();
    if (g_count != m_count)
    {
        GLogWarning("[PipelineBuild.VertexInputDiff] attr_count mismatch geometry=%u material=%u",
                    g_count,
                    m_count);
    }

    const uint32_t n = g_count < m_count ? g_count : m_count;
    for (uint32_t i = 0; i < n; ++i)
    {
        const VertexInputFormat *g = request.vil->GetConfig(i);
        const VertexInputFormat *m = material_vil->GetConfig(i);
        if (!g || !m)
            continue;

        if (g->attrib != m->attrib
         || g->format != m->format
         || g->vec_size != m->vec_size)
        {
            GLogWarning("[PipelineBuild.VertexInputDiff] index=%u geom(attrib=%u format=%d vec=%u) material(attrib=%u format=%d vec=%u)",
                        i,
                        static_cast<unsigned>(g->attrib),
                        static_cast<int>(g->format),
                        g->vec_size,
                        static_cast<unsigned>(m->attrib),
                        static_cast<int>(m->format),
                        m->vec_size);
        }
    }
}
}

GraphicsPipeline *MonolithicGraphicsPipelineBuilder::Build(const GraphicsPipelineBuildContext &context, const GraphicsPipelineBuildRequest &request)
{
    if (!context.device)
    {
        LogError("[MonolithicGraphicsPipelineBuilder] Build requires non-null context.device");
        return nullptr;
    }

    const bool pulling_enabled = IsPullingMaterial(request.material);

    if (!request.material || !request.render_format || !request.pipeline_data
     || (!pulling_enabled && !request.vil))
    {
        GLogError("[MonolithicGraphicsPipelineBuilder] Build invalid request: "
                  "material=%p render_format=%p vil=%p pipeline_data=%p",
                  static_cast<const void *>(request.material),
                  static_cast<const void *>(request.render_format),
                  static_cast<const void *>(request.vil),
                  static_cast<const void *>(request.pipeline_data));
        return nullptr;
    }

    const VIL *build_vil = pulling_enabled ? nullptr : request.vil;

    GraphicsPipelineData *pd = new GraphicsPipelineData(request.pipeline_data);
    pd->SetPrim(request.primitive, request.primitive_restart);
    pd->InitShaderStage(request.material->GetStageList());
    pd->InitVertexInputState(build_vil);

    LogPipelineVertexInputComparison(request);

    pd->SetColorAttachments(request.render_format->GetColorCount());
    pd->pipeline_info.layout = request.material->GetPipelineLayout();

    VkPipelineRenderingCreateInfoKHR rendering_ci{};
    rendering_ci.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_ci.colorAttachmentCount    = request.render_format->GetColorCount();
    rendering_ci.pColorAttachmentFormats = request.render_format->GetColorFormat().data();
    rendering_ci.depthAttachmentFormat   = request.render_format->GetDepthFormat();
    pd->pipeline_info.pNext              = &rendering_ci;
    pd->pipeline_info.renderPass         = VK_NULL_HANDLE;
    pd->pipeline_info.subpass            = 0;

    VkPipeline vk_pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(context.device->GetDevice(),
                                   context.pipeline_cache,
                                   1, &pd->pipeline_info,
                                   nullptr, &vk_pipeline) != VK_SUCCESS)
    {
        GLogError("[MonolithicGraphicsPipelineBuilder] vkCreateGraphicsPipelines failed");
        delete pd;
        return nullptr;
    }

    RenderTargetFormat::IncrVkCreateCount();

    AnsiString name = request.debug_name;
    if (name.IsEmpty())
        name = request.material->GetName();

    return new GraphicsPipeline(name, context.device->GetDevice(), vk_pipeline, build_vil, pd);
}

GraphicsPipeline *GplGraphicsPipelineBuilder::Build(const GraphicsPipelineBuildContext &context, const GraphicsPipelineBuildRequest &request)
{
    if (!context.device)
    {
        GLogError("[GplGraphicsPipelineBuilder] Build requires non-null context.device");
        return nullptr;
    }

    // ── One-time pool initialization ──────────────────────────────────────────
    std::call_once(init_flag_, [&]()
    {
        handle_cache_ = std::make_unique<GplLibraryHandleCache>();
        handle_cache_->Init(context.device->GetDevice(), context.pipeline_cache);
    });

    const bool pulling_enabled = IsPullingMaterial(request.material);
    const VIL *build_vil = pulling_enabled ? nullptr : request.vil;

    GraphicsPipelineBuildRequest build_request = request;
    build_request.vil = build_vil;

    // ── Compute per-library keys ──────────────────────────────────────────────
    const GplVertexInputKey    vi_key = BuildVertexInputKey(build_request.vil);
    const GplPreRasterKey      pr_key = BuildPreRasterKey(build_request);
    const GplFragmentShaderKey fs_key = BuildFragmentShaderKey(build_request);
    const GplFragmentOutputKey fo_key = BuildFragmentOutputKey(build_request.render_format);

    // ── Acquire (or create-and-cache) the four library handles ────────────────
    const VkPipeline vi_lib = handle_cache_->AcquireVertexInputLibrary(vi_key, build_request);
    const VkPipeline pr_lib = handle_cache_->AcquirePreRasterLibrary(pr_key, build_request);
    const VkPipeline fs_lib = handle_cache_->AcquireFragmentShaderLibrary(fs_key, build_request);
    const VkPipeline fo_lib = handle_cache_->AcquireFragmentOutputLibrary(fo_key, build_request);

    if (vi_lib == VK_NULL_HANDLE || pr_lib == VK_NULL_HANDLE
     || fs_lib == VK_NULL_HANDLE || fo_lib == VK_NULL_HANDLE)
    {
        GLogError("[GplGraphicsPipelineBuilder] One or more library stages failed to create "
                  "(VI=%s PR=%s FS=%s FO=%s), will fallback to monolithic",
                  vi_lib != VK_NULL_HANDLE ? "ok" : "FAIL",
                  pr_lib != VK_NULL_HANDLE ? "ok" : "FAIL",
                  fs_lib != VK_NULL_HANDLE ? "ok" : "FAIL",
                  fo_lib != VK_NULL_HANDLE ? "ok" : "FAIL");
        return nullptr;
    }

    // ── Link the four libraries into the final executable pipeline ─────────────
    const VkPipeline libs[4] = { vi_lib, pr_lib, fs_lib, fo_lib };

    VkPipelineLibraryCreateInfoKHR lib_ci{};
    lib_ci.sType        = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
    lib_ci.libraryCount = 4;
    lib_ci.pLibraries   = libs;

    const uint32_t color_count = request.render_format->GetColorCount();
    VkPipelineRenderingCreateInfoKHR rendering_ci{};
    rendering_ci.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_ci.pNext                   = nullptr;
    rendering_ci.colorAttachmentCount    = color_count;
    rendering_ci.pColorAttachmentFormats = request.render_format->GetColorFormat().data();
    rendering_ci.depthAttachmentFormat   = request.render_format->GetDepthFormat();

    lib_ci.pNext = &rendering_ci;

    VkGraphicsPipelineCreateInfo link_ci{};
    link_ci.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    link_ci.pNext      = &lib_ci;
    link_ci.flags      = VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
    link_ci.layout     = request.material->GetPipelineLayout();
    link_ci.renderPass = VK_NULL_HANDLE;

    VkDevice vk_device = context.device->GetDevice();

    VkPipeline final_pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(vk_device, context.pipeline_cache, 1,
                                  &link_ci, nullptr, &final_pipeline) != VK_SUCCESS)
    {
        GLogError("[GplGraphicsPipelineBuilder] Link step failed (vkCreateGraphicsPipelines)");
        return nullptr;
    }

    RenderTargetFormat::IncrVkCreateCount();

    AnsiString name = request.debug_name;
    if (name.IsEmpty())
        name = request.material->GetName();

    return new GraphicsPipeline(name, vk_device, final_pipeline, build_vil, nullptr);
}
}//namespace hgl::graph
