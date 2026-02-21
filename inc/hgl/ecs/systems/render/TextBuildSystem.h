#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class TextBuildSystem : public System
    {
    public:
        TextBuildSystem(const std::string& name = "TextBuildSystem");
        ~TextBuildSystem() override = default;

        void Update(float deltaTime) override;
    };
}
