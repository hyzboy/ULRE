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
        bool semantic_runtime_resolve_enabled = true;  // Default ON: deferred-MI primitives need runtime material resolution
        bool auto_transparency_enabled = false;
        bool use_real_alpha3d_enabled = true;
        float auto_transparency_near_distance = 3.0f;

    public:

        RenderPrimitiveCollectSystem(const std::string& name = "RenderPrimitiveCollectSystem");
        ~RenderPrimitiveCollectSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetCameraInfo(const graph::CameraInfo* info) { cameraInfo = info; }
        const graph::CameraInfo* GetCameraInfo() const { return cameraInfo; }
        void SetSemanticRuntimeResolveEnabled(bool enabled) { semantic_runtime_resolve_enabled = enabled; }
        bool GetSemanticRuntimeResolveEnabled() const { return semantic_runtime_resolve_enabled; }
        void SetAutoTransparencyEnabled(bool enabled) { auto_transparency_enabled = enabled; }
        bool GetAutoTransparencyEnabled() const { return auto_transparency_enabled; }
        void SetUseRealAlpha3DEnabled(bool enabled) { use_real_alpha3d_enabled = enabled; }
        bool GetUseRealAlpha3DEnabled() const { return use_real_alpha3d_enabled; }
        void SetAutoTransparencyNearDistance(float v) { auto_transparency_near_distance = v; }
        float GetAutoTransparencyNearDistance() const { return auto_transparency_near_distance; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs

