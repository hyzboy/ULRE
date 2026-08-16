#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class RenderFrameUBOSyncSystem : public System
    {
    public:
        RenderFrameUBOSyncSystem(const std::string& name = "RenderFrameUBOSyncSystem");
        ~RenderFrameUBOSyncSystem() override = default;

        void Update(float deltaTime) override;
    };
}
