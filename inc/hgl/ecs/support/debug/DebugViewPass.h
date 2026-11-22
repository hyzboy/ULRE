#pragma once

#include<hgl/type/String.h>
#include<hgl/mtl/DebugViewMode.h>

namespace hgl::graph
{
    class IRenderTarget;
    class RenderCmdBuffer;
}

namespace hgl::ecs
{
    class ECSContext;

    class DebugViewPass
    {
    public:
        static bool RequestMode(graph::mtl::DebugViewMode);
        static graph::mtl::DebugViewMode GetRequestedMode();
        static graph::mtl::DebugViewMode GetAppliedMode();
        static uint64_t GetTargetID();
        static uint32_t GetTrackedCount();
        static AnsiString GetStatusJSON();
        static void Record(ECSContext *,graph::RenderCmdBuffer *,graph::IRenderTarget *);
        static void ProcessSubmitted(graph::IRenderTarget *,uint64_t frame_id);
        static void SubmissionFailed(graph::IRenderTarget *,uint64_t frame_id);
        static void Reset(ECSContext *);
    };
}
