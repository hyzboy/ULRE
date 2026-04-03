#include<hgl/vk/pipeline/VKGraphicsPipelineBuilder.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/pipeline/VKGplLibraryHandleCache.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineBuildRequest.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/vk/pipeline/VKRenderFormat.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
namespace
{
GraphicsPipeline *CreateMonolithicFromRequest(const char *tag, const GraphicsPipelineBuildRequest &request)
{
    if (!request.material || !request.render_format || !request.vil || !request.pipeline_data)
    {
        GLogError("[%s] Build invalid request: material=%p render_format=%p vil=%p pipeline_data=%p",
                  tag,
                  static_cast<const void *>(request.material),
                  static_cast<const void *>(request.render_format),
                  static_cast<const void *>(request.vil),
                  static_cast<const void *>(request.pipeline_data));
        return nullptr;
    }

    AnsiString pipeline_name = request.debug_name;
    if (pipeline_name.IsEmpty())
        pipeline_name = request.material->GetName();

    RenderFormat *render_format = const_cast<RenderFormat *>(request.render_format);

    GraphicsPipeline *pipeline = render_format->CreatePipeline(
        pipeline_name,
        request.material->GetStageList(),
        request.material->GetPipelineLayout(),
        request.vil,
        request.pipeline_data,
        request.primitive,
        request.primitive_restart);

    if (!pipeline)
    {
        GLogError("[%s] Build failed: name=%s", tag, pipeline_name.c_str());
        return nullptr;
    }

    return pipeline;
}
}

GraphicsPipeline *MonolithicGraphicsPipelineBuilder::Build(const GraphicsPipelineBuildContext &context, const GraphicsPipelineBuildRequest &request)
{
    if (!context.device)
    {
        LogError("[MonolithicGraphicsPipelineBuilder] Build requires non-null context.device");
        return nullptr;
    }

    return CreateMonolithicFromRequest("MonolithicGraphicsPipelineBuilder", request);
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
        library_pool_ = std::make_unique<GplLibraryHandleCache>();
        library_pool_->Init(context.device->GetDevice(), context.pipeline_cache);
    });

    // ── Compute per-library keys ──────────────────────────────────────────────
    const GplVertexInputKey    vi_key = BuildVertexInputKey(request.vil);
    const GplPreRasterKey      pr_key = BuildPreRasterKey(request);
    const GplFragmentShaderKey fs_key = BuildFragmentShaderKey(request);
    const GplFragmentOutputKey fo_key = BuildFragmentOutputKey(request.render_format);

    // ── Acquire (or create-and-cache) the four library handles ────────────────
    const VkPipeline vi_lib = library_pool_->AcquireVI(vi_key, request);
    const VkPipeline pr_lib = library_pool_->AcquirePR(pr_key, request);
    const VkPipeline fs_lib = library_pool_->AcquireFS(fs_key, request);
    const VkPipeline fo_lib = library_pool_->AcquireFO(fo_key, request);

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
    rendering_ci.pColorAttachmentFormats = request.render_format->GetColorFormat().GetData();
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

    RenderFormat::IncrVkCreateCount();

    AnsiString name = request.debug_name;
    if (name.IsEmpty())
        name = request.material->GetName();

    return new GraphicsPipeline(name, vk_device, final_pipeline, request.vil, nullptr);
}
}//namespace hgl::graph
