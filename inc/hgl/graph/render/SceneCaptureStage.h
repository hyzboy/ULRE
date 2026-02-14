#pragma once

#include <functional>

#include<hgl/graph/render/RenderStagePipeline.h>

namespace hgl::graph
{
    class SceneCaptureStage final : public RenderStage
    {
    public:

        enum class CaptureTarget
        {
            Texture2D,
            CubeMap,
            Deferred
        };

        using CaptureFunc = std::function<bool(RenderStageContext &)>;

    private:

        CaptureFunc   capture_func;
        bool          only_once = true;
        bool          captured = false;
        CaptureTarget capture_target = CaptureTarget::Texture2D;

    public:

        SceneCaptureStage() = default;
        explicit SceneCaptureStage(CaptureFunc func,bool once = true,CaptureTarget target = CaptureTarget::Texture2D)
            : capture_func(std::move(func)), only_once(once), capture_target(target)
        {
        }

        const char *GetName() const override { return "SceneCapture"; }

        void SetCaptureFunc(CaptureFunc func){ capture_func = std::move(func); }
        void SetCaptureOnce(bool once){ only_once = once; }
        void ResetCapture(){ captured = false; }

        void SetCaptureTarget(CaptureTarget target){ capture_target = target; }
        CaptureTarget GetCaptureTarget() const { return capture_target; }

        void Execute(RenderStageContext &ctx) override
        {
            if(!capture_func)
                return;

            if(only_once && captured)
                return;

            if(capture_func(ctx))
                captured = true;
        }
    };
}//namespace hgl::graph
