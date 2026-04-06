#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
    }
}

namespace hgl::ecs
{
    class ECSContext;

    /**
     * RenderPrimitiveCollectSystem
     *
     * Collects primitive render items for the current frame.
     */
    class RenderPrimitiveCollectSystem : public System
    {
    private:

        ECSContext* world = nullptr;
        const graph::CameraInfo* cameraInfo = nullptr;
        bool semantic_runtime_resolve_enabled = false;

    public:

        RenderPrimitiveCollectSystem(const std::string& name = "RenderPrimitiveCollectSystem");
        ~RenderPrimitiveCollectSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetCameraInfo(const graph::CameraInfo* info) { cameraInfo = info; }
        const graph::CameraInfo* GetCameraInfo() const { return cameraInfo; }
        void SetSemanticRuntimeResolveEnabled(bool enabled) { semantic_runtime_resolve_enabled = enabled; }
        bool GetSemanticRuntimeResolveEnabled() const { return semantic_runtime_resolve_enabled; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs

