#include <hgl/ecs/core/InstancedPrimitiveRenderItem.h>

namespace hgl::ecs
{
    InstancedPrimitiveRenderItem::InstancedPrimitiveRenderItem(
        EntityID ent_id,
        std::shared_ptr<TransformComponent> trans,
        std::shared_ptr<InstancedPrimitiveComponent> prim,
        std::shared_ptr<MaterialComponent> mat,
        ECSContext *ctx)
        : PrimitiveRenderItem(ent_id, trans, prim, mat, ctx)
        , instancedComp(prim)
    {
    }

    uint32_t InstancedPrimitiveRenderItem::GetInstanceCount() const
    {
        return instancedComp ? instancedComp->GetInstanceCount() : 0;
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetL2WBuffer() const
    {
        return instancedComp ? instancedComp->GetL2WBuffer() : nullptr;
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetL2WIndexBuffer() const
    {
        return instancedComp ? instancedComp->GetL2WIndexBuffer() : nullptr;
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetMeshDrawParamsBuffer() const
    {
        return instancedComp ? instancedComp->GetMeshDrawParamsBuffer() : nullptr;
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetMaterialDataRowsBuffer() const
    {
        return instancedComp ? instancedComp->GetMaterialDataRowsBuffer() : nullptr;
    }

    hgl::graph::IndirectMeshTaskBuffer *InstancedPrimitiveRenderItem::GetIndirectMeshTaskBuffer() const
    {
        return instancedComp ? instancedComp->GetIndirectMeshTaskBuffer() : nullptr;
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetIndirectCountBuffer() const
    {
        return instancedComp ? instancedComp->GetIndirectCountBuffer() : nullptr;
    }

    VkDeviceSize InstancedPrimitiveRenderItem::GetIndirectCountOffset() const
    {
        return instancedComp ? instancedComp->GetIndirectCountOffset() : 0;
    }

    bool InstancedPrimitiveRenderItem::IsGPUDriven() const
    {
        return instancedComp ? instancedComp->IsGPUDriven() : false;
    }

    bool InstancedPrimitiveRenderItem::IsIndirect() const
    {
        return instancedComp ? instancedComp->IsIndirect() : false;
    }
}//namespace hgl::ecs
