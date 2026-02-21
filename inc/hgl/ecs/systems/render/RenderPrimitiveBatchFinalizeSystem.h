#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class RenderPrimitiveBatchFinalizeSystem : public System
    {
    public:
        RenderPrimitiveBatchFinalizeSystem(const std::string& name = "RenderPrimitiveBatchFinalizeSystem");
        ~RenderPrimitiveBatchFinalizeSystem() override = default;

        void Update(float deltaTime) override;
    };
}
