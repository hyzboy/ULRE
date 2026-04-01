#include<hgl/vk/pipeline/VKLinkBackend.h>
#include<hgl/vk/pipeline/VKGplRequest.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
Pipeline *MonolithicLinkBackend::Build(const PipelineBuildContext &context, const GplPipelineRequest &request)
{
    (void)context;
    (void)request;

    LogWarning("[MonolithicLinkBackend] Build skeleton reached before implementation");
    return nullptr;
}

Pipeline *GplLinkBackend::Build(const PipelineBuildContext &context, const GplPipelineRequest &request)
{
    (void)context;
    (void)request;

    LogWarning("[GplLinkBackend] Build skeleton reached before implementation");
    return nullptr;
}
}//namespace hgl::graph