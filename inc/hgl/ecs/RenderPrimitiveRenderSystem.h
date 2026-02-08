#pragma once

#include<hgl/ecs/System.h>

namespace hgl::ecs
{
    class ECSContext;
    class RenderPrimitiveSystem;

    /**
     * RenderPrimitiveRenderSystem
     *
     * Render-only system for PrimitiveComponent batches.
     * Relies on RenderPrimitiveSystem to build batches.
     */
    class RenderPrimitiveRenderSystem : public System
    {
    private:

        ECSContext* world = nullptr;
        bool warned_missing_batch_system = false;

    public:

        RenderPrimitiveRenderSystem(const std::string& name = "RenderPrimitiveRenderSystem");
        ~RenderPrimitiveRenderSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

        void Update(float deltaTime) override;
        void Render(graph::RenderCmdBuffer* cmdBuffer, float deltaTime) override;
    };
}//namespace hgl::ecs
