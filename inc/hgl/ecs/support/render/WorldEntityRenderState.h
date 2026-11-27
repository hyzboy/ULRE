#pragma once

#include <hgl/ecs/support/storage/EntityGPUTable.h>
#include <hgl/ecs/support/storage/EntityRenderIDTable.h>

namespace hgl::ecs
{
    /**
     * CPU-side, entity-indexed render state derived from ECS entity rows.
     *
     * This owns the source tables only; WorldGPUResources owns their device
     * mirrors and uploads.
     */
    class WorldEntityRenderState
    {
    private:
        EntityGPUTable entity_gpu_table;
        EntityRenderIDTable entity_render_id_table;

    public:
        WorldEntityRenderState() = default;
        WorldEntityRenderState(const WorldEntityRenderState &) = delete;
        WorldEntityRenderState &operator=(const WorldEntityRenderState &) = delete;

        EntityGPUTable &EntityGPU() { return entity_gpu_table; }
        const EntityGPUTable &EntityGPU() const { return entity_gpu_table; }

        EntityRenderIDTable &EntityRenderIDs() { return entity_render_id_table; }
        const EntityRenderIDTable &EntityRenderIDs() const { return entity_render_id_table; }

        void ActivateEntityRow(EntityID id);
        void DeactivateEntityRow(EntityID id);
    };
}
