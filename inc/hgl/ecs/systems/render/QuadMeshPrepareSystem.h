#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class ECSContext;

    class QuadMeshPrepareSystem : public System
    {
    private:

        ECSContext *world = nullptr;

    public:

        QuadMeshPrepareSystem(const std::string& name = "QuadMeshPrepareSystem");
        ~QuadMeshPrepareSystem() override = default;

    public:

        void SetWorld(ECSContext *w) { world = w; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
