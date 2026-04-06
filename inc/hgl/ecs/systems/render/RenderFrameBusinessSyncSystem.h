#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class RenderFrameBusinessSyncSystem : public System
    {
    public:
        RenderFrameBusinessSyncSystem(const std::string& name = "RenderFrameBusinessSyncSystem");
        ~RenderFrameBusinessSyncSystem() override = default;

        void Update(float deltaTime) override;
    };
}
