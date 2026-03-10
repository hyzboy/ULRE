#pragma once

#include <hgl/ecs/core/System.h>

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
     * AssetInstanceCollectSystem
     *
     * Gather-phase system for entities that carry an AssetInstanceComponent.
     * For each entity it finds all Primitives in the referenced StaticMesh
     * and inserts one AssetPrimitiveRenderItem per Primitive into
     * RenderFrameCache::renderItems — exactly how RenderPrimitiveCollectSystem
     * handles PrimitiveComponent entities.
     *
     * Registered under the "AssetInstance" SystemGroup so that it is only
     * installed when at least one AssetInstanceComponent exists in the world.
     */
    class AssetInstanceCollectSystem : public System
    {
    private:

        ECSContext*               world      = nullptr;
        const graph::CameraInfo*  cameraInfo = nullptr;

    public:

        AssetInstanceCollectSystem(const std::string& name = "AssetInstanceCollectSystem");
        ~AssetInstanceCollectSystem() override = default;

    public:

        void SetWorld(ECSContext* w)                        { world = w; }
        void SetCameraInfo(const graph::CameraInfo* info)   { cameraInfo = info; }
        const graph::CameraInfo* GetCameraInfo() const      { return cameraInfo; }

        void Update(float deltaTime) override;
    };

}//namespace hgl::ecs
