#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class RenderPrimitiveCullSystem : public System
    {
    public:
        RenderPrimitiveCullSystem(const std::string& name = "RenderPrimitiveCullSystem");
        ~RenderPrimitiveCullSystem() override = default;

        void Update(float deltaTime) override;
    };
}
