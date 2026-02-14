#pragma once

#include<hgl/ecs/core/System.h>

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
     * TextRenderSubmitSystem
     *
     * Submits text draw calls directly, bypassing Primitive collect/batch.
     */
    class TextRenderSubmitSystem : public System
    {
    private:

        ECSContext* world = nullptr;

    public:

        TextRenderSubmitSystem(const std::string& name = "TextRenderSubmitSystem");
        ~TextRenderSubmitSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

        void Render(graph::RenderCmdBuffer* cmdBuffer, float deltaTime) override;
    };
}//namespace hgl::ecs

