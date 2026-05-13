#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class ECSContext;

    class PrimitiveBindingCommitSystem : public System
    {
    private:

        ECSContext *world = nullptr;

    public:

        PrimitiveBindingCommitSystem(const std::string &name = "PrimitiveBindingCommitSystem");
        ~PrimitiveBindingCommitSystem() override = default;

        void SetWorld(ECSContext *w) { world = w; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
