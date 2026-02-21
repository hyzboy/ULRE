#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class RenderPrimitiveBatchBuildSystem : public System
    {
    public:
        RenderPrimitiveBatchBuildSystem(const std::string& name = "RenderPrimitiveBatchBuildSystem");
        ~RenderPrimitiveBatchBuildSystem() override = default;

        void Update(float deltaTime) override;
    };
}
