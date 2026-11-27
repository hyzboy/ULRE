#include <hgl/ecs/support/render/WorldEntityRenderState.h>

namespace hgl::ecs
{
    void WorldEntityRenderState::ActivateEntityRow(EntityID id)
    {
        entity_gpu_table.Activate(id);
    }

    void WorldEntityRenderState::DeactivateEntityRow(EntityID id)
    {
        entity_gpu_table.Deactivate(id);
        entity_render_id_table.Clear(id);
    }
}
