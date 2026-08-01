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
    class PrimitiveComponent;
    class MaterialComponent;

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
        bool ResolveMaterialProgramForPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                                const std::shared_ptr<MaterialComponent> &material_comp);
        bool ResolveRuntimePipelineForPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                                const std::shared_ptr<MaterialComponent> &material_comp);
        bool MaterializeRecipeRowsForPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                               const std::shared_ptr<MaterialComponent> &material_comp);

    public:

        RenderPrimitiveCollectSystem(const std::string& name = "RenderPrimitiveCollectSystem");
        ~RenderPrimitiveCollectSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetCameraInfo(const graph::CameraInfo* info) { cameraInfo = info; }
        const graph::CameraInfo* GetCameraInfo() const { return cameraInfo; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
