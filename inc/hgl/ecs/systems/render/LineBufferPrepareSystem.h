#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class ECSContext;

    class LineBufferPrepareSystem : public System
    {
    private:
        ECSContext* world = nullptr;

    public:
        explicit LineBufferPrepareSystem(const std::string& name = "LineBufferPrepareSystem");
        ~LineBufferPrepareSystem() override = default;

        void SetWorld(ECSContext* w) { world = w; }
        void Update(float deltaTime) override;
    };
}
