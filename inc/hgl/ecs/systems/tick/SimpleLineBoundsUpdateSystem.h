#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class ECSContext;

    class SimpleLineBoundsUpdateSystem : public System
    {
    private:
        ECSContext* world = nullptr;

    public:
        explicit SimpleLineBoundsUpdateSystem(const AnsiString& name = "SimpleLineBoundsUpdateSystem");
        ~SimpleLineBoundsUpdateSystem() override = default;

        void SetWorld(ECSContext* w) { world = w; }
        void Update(float deltaTime) override;
    };
}
