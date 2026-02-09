#pragma once

#include<hgl/ecs/System.h>

namespace hgl
{
    namespace graph
    {
        class RenderCmdBuffer;
    }
}

namespace hgl::ecs
{
    class ECSContext;

    /**
     * RenderPrimitiveSubmitSystem
     *
     * Submits draw calls for batched primitive render items.
     */
    class RenderPrimitiveSubmitSystem : public System
    {
    private:

        ECSContext* world = nullptr;

    public:

        RenderPrimitiveSubmitSystem(const std::string& name = "RenderPrimitiveSubmitSystem");
        ~RenderPrimitiveSubmitSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

        void Render(graph::RenderCmdBuffer* cmdBuffer, float deltaTime) override;
    };
}//namespace hgl::ecs
