#include<hgl/vk/pipeline/VKGraphicsPipelineBuilder.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/pipeline/VKGplLibraryHandleCache.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineBuildRequest.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/vk/pipeline/VKRenderTargetFormat.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/log/Log.h>
#include<string>

namespace hgl::graph{

namespace
{
struct PipelineStageComposition
{
    bool vertex = false;
    bool tess_ctrl = false;
    bool tess_eval = false;
    bool geometry = false;
    bool fragment = false;
    bool task = false;
    bool mesh = false;
    bool compute = false;
    uint32_t count = 0;
};

static PipelineStageComposition BuildPipelineStageComposition(const ShaderMaterialProgram *material)
{
    PipelineStageComposition sc;

    if (!material)
        return sc;

    const auto &stages = material->GetStageList();
    sc.count = static_cast<uint32_t>(stages.size());

    for (const auto &s : stages)
    {
        switch (s.stage)
        {
            case VK_SHADER_STAGE_VERTEX_BIT:                    sc.vertex = true; break;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:      sc.tess_ctrl = true; break;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:   sc.tess_eval = true; break;
            case VK_SHADER_STAGE_GEOMETRY_BIT:                  sc.geometry = true; break;
            case VK_SHADER_STAGE_FRAGMENT_BIT:                  sc.fragment = true; break;
            case VK_SHADER_STAGE_TASK_BIT_EXT:                  sc.task = true; break;
            case VK_SHADER_STAGE_MESH_BIT_EXT:                  sc.mesh = true; break;
            case VK_SHADER_STAGE_COMPUTE_BIT:                   sc.compute = true; break;
            default: break;
        }
    }

    return sc;
}

static bool HasLegacyVertexPathStages(const PipelineStageComposition &sc)
{
    return sc.vertex || sc.tess_ctrl || sc.tess_eval || sc.geometry;
}

static void AppendStageName(std::string &out, const bool enabled, const char *name)
{
    if (!enabled || !name || !*name)
        return;

    if (!out.empty())
        out += "|";

    out += name;
}

static std::string FormatPipelineStageComposition(const PipelineStageComposition &sc)
{
    std::string out;
    AppendStageName(out, sc.vertex, "Vertex");
    AppendStageName(out, sc.tess_ctrl, "TessCtrl");
    AppendStageName(out, sc.tess_eval, "TessEval");
    AppendStageName(out, sc.geometry, "Geometry");
    AppendStageName(out, sc.task, "Task");
    AppendStageName(out, sc.mesh, "Mesh");
    AppendStageName(out, sc.fragment, "Fragment");
    AppendStageName(out, sc.compute, "Compute");

    if (out.empty())
        out = "<none>";

    return out;
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

static void LogPipelineStageAndLayoutDiagnostics(const GraphicsPipelineBuildRequest &request,
                                                 const bool vertex_input_ignored)
{
    if (!request.material)
        return;

    const PipelineStageComposition sc = BuildPipelineStageComposition(request.material);
    const std::string stage_text = FormatPipelineStageComposition(sc);

    GLogInfo("[PipelineBuild.StageCompose] material=%s request_mode=%s effective_mode=%s stages=%s stage_count=%u",
             request.material->GetName().c_str(),
             GetGraphicsPipelineRequestModeName(request.request_mode),
             "Vertex",
             stage_text.c_str(),
             sc.count);

    if (sc.task || sc.mesh)
    {
        GLogError("[PipelineBuild.StageCompose] Graphics pipeline build no longer supports Task/Mesh stages");
    }

    if (sc.task && !sc.mesh)
    {
        GLogError("[PipelineBuild.StageCompose] Task stage present without mesh stage");
    }

    VkPipelineLayout layout = request.material->GetPipelineLayout();
    if (!layout)
    {
        GLogError("[PipelineBuild.Layout] material=%s pipeline layout is null",
                  request.material->GetName().c_str());
        return;
    }

    const uint32_t color_count = request.render_format ? request.render_format->GetColorCount() : 0;
    const int depth_format = request.render_format ? static_cast<int>(request.render_format->GetDepthFormat()) : int(VK_FORMAT_UNDEFINED);

    GLogInfo("[PipelineBuild.Layout] material=%s layout=%p color_count=%u depth_format=%d vertex_input_ignored=%s",
             request.material->GetName().c_str(),
             static_cast<void *>(layout),
             color_count,
             depth_format,
             vertex_input_ignored ? "yes" : "no");
}

static void LogPipelineVertexInputComparison(const GraphicsPipelineBuildRequest &request,
                                             const bool vertex_input_ignored)
{
    if (!request.material)
        return;

    if (vertex_input_ignored)
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

static bool ValidateGraphicsPipelineStages(const GraphicsPipelineBuildContext &context,
                                           const GraphicsPipelineBuildRequest &request)
{
    if (!context.device || !request.material)
        return true;

    const PipelineStageComposition sc = BuildPipelineStageComposition(request.material);

    if (sc.task || sc.mesh)
    {
        GLogError("[PipelineBuild.StageCompose] Graphics pipeline build rejected Task/Mesh stages: material=%s",
                  request.material->GetName().c_str());
        return false;
    }

    if (!HasLegacyVertexPathStages(sc))
    {
        GLogError("[PipelineBuild.StageCompose] Graphics pipeline build requires Vertex/Tess/Geometry stages: material=%s",
                  request.material->GetName().c_str());
        return false;
    }

    if (!sc.fragment)
        return true;

    return true;
}
}

GraphicsPipeline *MonolithicGraphicsPipelineBuilder::Build(const GraphicsPipelineBuildContext &context, const GraphicsPipelineBuildRequest &request)
{
    if (!context.device)
    {
        LogError("[MonolithicGraphicsPipelineBuilder] Build requires non-null context.device");
        return nullptr;
    }

    const bool vertex_input_ignored = IsVertexInputIgnored(request);

    if (!request.material || !request.render_format || !request.pipeline_data
     || (!vertex_input_ignored && !request.vil))
    {
        GLogError("[MonolithicGraphicsPipelineBuilder] Build invalid request: "
                  "material=%p render_format=%p vil=%p pipeline_data=%p",
                  static_cast<const void *>(request.material),
                  static_cast<const void *>(request.render_format),
                  static_cast<const void *>(request.vil),
                  static_cast<const void *>(request.pipeline_data));
        return nullptr;
    }

    if (!ValidateGraphicsPipelineStages(context, request))
        return nullptr;

    const VIL *build_vil = vertex_input_ignored ? nullptr : request.vil;

    GraphicsPipelineData *pd = new GraphicsPipelineData(request.pipeline_data);
    pd->SetPrim(request.primitive, request.primitive_restart);
    pd->InitShaderStage(request.material->GetStageList());
    pd->InitVertexInputState(build_vil);

    LogPipelineStageAndLayoutDiagnostics(request, vertex_input_ignored);
    LogPipelineVertexInputComparison(request, vertex_input_ignored);

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

    GraphicsPipeline *result = new GraphicsPipeline(name,
                                                    context.device->GetDevice(),
                                                    vk_pipeline,
                                                    build_vil,
                                                    pd);

    result->SetDebugRenderingSignature(rendering_ci.depthAttachmentFormat,
                                       request.render_format->GetColorCount(),
                                       false);

    GLogInfo("[MonolithicGraphicsPipelineBuilder] Created pipeline handle=0x%llx material=%s depthFormat=%d stencilFormat=%d colorCount=%u",
             (unsigned long long)(uintptr_t)vk_pipeline,
             request.material->GetName().c_str(),
             (int)rendering_ci.depthAttachmentFormat,
             (int)rendering_ci.stencilAttachmentFormat,
             request.render_format->GetColorCount());

    return result;
}

GraphicsPipeline *GplGraphicsPipelineBuilder::Build(const GraphicsPipelineBuildContext &context, const GraphicsPipelineBuildRequest &request)
{
    if (!context.device)
    {
        GLogError("[GplGraphicsPipelineBuilder] Build requires non-null context.device");
        return nullptr;
    }

    const bool vertex_input_ignored = IsVertexInputIgnored(request);

    if (!request.material || !request.render_format || !request.pipeline_data
     || (!vertex_input_ignored && !request.vil))
    {
        GLogError("[GplGraphicsPipelineBuilder] Build invalid request: "
                  "material=%p render_format=%p vil=%p pipeline_data=%p",
                  static_cast<const void *>(request.material),
                  static_cast<const void *>(request.render_format),
                  static_cast<const void *>(request.vil),
                  static_cast<const void *>(request.pipeline_data));
        return nullptr;
    }

    if (!ValidateGraphicsPipelineStages(context, request))
        return nullptr;

    LogPipelineStageAndLayoutDiagnostics(request, vertex_input_ignored);
    LogPipelineVertexInputComparison(request, vertex_input_ignored);

    // ── One-time pool initialization ──────────────────────────────────────────
    std::call_once(init_flag_, [&]()
    {
        handle_cache_ = std::make_unique<GplLibraryHandleCache>();
        handle_cache_->Init(context.device->GetDevice(), context.pipeline_cache);
    });

    const VIL *build_vil = vertex_input_ignored ? nullptr : request.vil;

    GraphicsPipelineBuildRequest build_request = request;
    GraphicsPipelineData prepared_pipeline_data(request.pipeline_data);
    prepared_pipeline_data.SetPrim(request.primitive, request.primitive_restart);

    build_request.pipeline_data = &prepared_pipeline_data;
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
    lib_ci.pNext        = nullptr;
    lib_ci.libraryCount = 4;
    lib_ci.pLibraries   = libs;

    const uint32_t color_count = request.render_format->GetColorCount();
    VkPipelineRenderingCreateInfoKHR rendering_ci{};
    rendering_ci.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_ci.pNext                   = nullptr;
    rendering_ci.colorAttachmentCount    = color_count;
    rendering_ci.pColorAttachmentFormats = request.render_format->GetColorFormat().data();
    rendering_ci.depthAttachmentFormat   = request.render_format->GetDepthFormat();
    rendering_ci.pNext                   = &lib_ci;

    VkGraphicsPipelineCreateInfo link_ci{};
    link_ci.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    link_ci.pNext      = &rendering_ci;
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

    GraphicsPipeline *result = new GraphicsPipeline(name,
                                                    vk_device,
                                                    final_pipeline,
                                                    build_vil,
                                                    nullptr);

    result->SetDebugRenderingSignature(rendering_ci.depthAttachmentFormat,
                                       request.render_format->GetColorCount(),
                                       true);

    GLogInfo("[GplGraphicsPipelineBuilder] Linked pipeline handle=0x%llx material=%s depthFormat=%d stencilFormat=%d colorCount=%u",
             (unsigned long long)(uintptr_t)final_pipeline,
             request.material->GetName().c_str(),
             (int)rendering_ci.depthAttachmentFormat,
             (int)rendering_ci.stencilAttachmentFormat,
             request.render_format->GetColorCount());

    return result;
}
}//namespace hgl::graph
