#pragma once

#include <hgl/ecs/core/PrimitiveRenderItem.h>
#include <hgl/ecs/components/InstancedPrimitiveComponent.h>

namespace hgl::ecs
{
    /**
     * InstancedPrimitiveRenderItem - specialized RenderItem for InstancedPrimitiveComponent
     *
     * Represents a group of N instances rendered from a single mesh and material.
     */
    class InstancedPrimitiveRenderItem : public PrimitiveRenderItem
    {
    private:
        std::shared_ptr<InstancedPrimitiveComponent> instancedComp;

    public:
        InstancedPrimitiveRenderItem(
            EntityID ent_id,
            std::shared_ptr<TransformComponent> trans,
            std::shared_ptr<InstancedPrimitiveComponent> prim,
            std::shared_ptr<MaterialComponent> mat = nullptr,
            ECSContext *ctx = nullptr);

        ~InstancedPrimitiveRenderItem() override = default;

        std::shared_ptr<InstancedPrimitiveComponent> GetInstancedPrimitiveComponent() const
        {
            return instancedComp;
        }

        uint32_t GetInstanceCount() const;
        hgl::graph::DeviceBuffer *GetL2WBuffer() const;
        hgl::graph::DeviceBuffer *GetL2WIndexBuffer() const;
        hgl::graph::DeviceBuffer *GetMeshDrawParamsBuffer() const;
        hgl::graph::DeviceBuffer *GetMaterialDataRowsBuffer() const;
        hgl::graph::IndirectMeshTaskBuffer *GetIndirectMeshTaskBuffer() const;
        hgl::graph::DeviceBuffer *GetIndirectCountBuffer() const;
        VkDeviceSize GetIndirectCountOffset() const;
        bool IsGPUDriven() const;
        bool IsIndirect() const;
    };
}//namespace hgl::ecs
