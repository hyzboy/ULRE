#pragma once

#include<hgl/ecs/core/System.h>
#include<glm/glm.hpp>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
        class Primitive;
        class Material;
        class MaterialInstance;
        class Pipeline;
    }
}

namespace hgl::ecs
{
    class TransformComponent;
    class BillboardComponent;

    /**
     * BillboardRenderSystem
     *
     * Handles billboard-specific rendering operations and updates.
     *
     * This system can be used to:
     * - Update billboard properties dynamically
     * - Apply billboard-specific transformations
     * - Handle material updates for billboards
     *
     * Note: Billboard rendering itself is handled by RenderPrimitiveBatchSystem,
     * which processes BillboardComponent (which derives from PrimitiveComponent).
     */
    class BillboardRenderSystem : public System
    {
    private:

        class ECSContext* world = nullptr;
        const graph::CameraInfo* cameraInfo = nullptr;

    public:

        BillboardRenderSystem(const std::string& name = "BillboardRenderSystem");
        ~BillboardRenderSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetCameraInfo(const graph::CameraInfo* info) { cameraInfo = info; }

        const graph::CameraInfo* GetCameraInfo() const { return cameraInfo; }

    public:

        void Update(float deltaTime) override;

    private:

        // Helper methods for billboard-specific operations
        bool UpdateBillboardRotation(BillboardComponent* billboard,
                                    TransformComponent* transform,
                                    float deltaTime);
    };
}//namespace hgl::ecs
