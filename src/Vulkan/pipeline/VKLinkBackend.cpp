#include<hgl/vk/pipeline/VKLinkBackend.h>
#include<hgl/vk/pipeline/VKGplRequest.h>
#include<hgl/vk/VKRenderFormat.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
namespace
{
Pipeline *CreateMonolithicFromRequest(const char *tag, const GplPipelineRequest &request)
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

    Pipeline *pipeline = render_format->CreatePipeline(
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

Pipeline *MonolithicLinkBackend::Build(const PipelineBuildContext &context, const GplPipelineRequest &request)
{
    if (!context.device)
    {
        LogError("[MonolithicLinkBackend] Build requires non-null context.device");
        return nullptr;
    }

    return CreateMonolithicFromRequest("MonolithicLinkBackend", request);
}

Pipeline *GplLinkBackend::Build(const PipelineBuildContext &context, const GplPipelineRequest &request)
{
    if (!context.device)
    {
        LogError("[GplLinkBackend] Build requires non-null context.device");
        return nullptr;
    }

    static bool warned = false;
    if (!warned)
    {
        LogWarning("[GplLinkBackend] Minimal runnable mode active: fallback to monolithic create until GPL library/link backend is implemented");
        warned = true;
    }

    return CreateMonolithicFromRequest("GplLinkBackend", request);
}
}//namespace hgl::graph