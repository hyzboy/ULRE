#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class TextCollectSystem : public System
    {
    public:
        TextCollectSystem(const std::string& name = "TextCollectSystem");
        ~TextCollectSystem() override = default;

        void Update(float deltaTime) override;
    };
}
