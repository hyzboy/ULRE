#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class ECSContext;

    class LineBoundsUpdateSystem : public System
    {
    private:
        ECSContext* world = nullptr;

    public:
        explicit LineBoundsUpdateSystem(const std::string& name = "LineBoundsUpdateSystem");
        ~LineBoundsUpdateSystem() override = default;

        void SetWorld(ECSContext* w) { world = w; }
        void Update(float deltaTime) override;
    };
}
