#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class RenderPrimitiveSortSystem : public System
    {
    public:
        RenderPrimitiveSortSystem(const std::string& name = "RenderPrimitiveSortSystem");
        ~RenderPrimitiveSortSystem() override = default;

        void Update(float deltaTime) override;
    };
}
