#include<hgl/vk/pipeline/VKLinkBackend.h>
#include<hgl/vk/pipeline/VKGplRequest.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
Pipeline *MonolithicLinkBackend::Build(const PipelineBuildContext &context, const GplPipelineRequest &request)
{
    if (!context.device)
    {
        LogError("[MonolithicLinkBackend] Build requires non-null context.device");
        return nullptr;
    }

    static bool warned = false;
    if (!warned)
    {
        LogWarning("[MonolithicLinkBackend] Build skeleton not implemented yet, debug_name=%s",
                   request.debug_name.c_str());
        warned = true;
    }

    return nullptr;
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
        LogWarning("[GplLinkBackend] Build skeleton not implemented yet, debug_name=%s",
                   request.debug_name.c_str());
        warned = true;
    }

    return nullptr;
}
}//namespace hgl::graph