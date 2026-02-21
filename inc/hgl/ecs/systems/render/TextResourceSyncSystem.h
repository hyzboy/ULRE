#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class TextResourceSyncSystem : public System
    {
    public:
        TextResourceSyncSystem(const std::string& name = "TextResourceSyncSystem");
        ~TextResourceSyncSystem() override = default;

        void Update(float deltaTime) override;
    };
}
