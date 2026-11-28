#include <hgl/ecs/systems/tick/SpatialTransformSystem.h>

namespace hgl::ecs
{
    SpatialTransformSystem::SpatialTransformSystem(WorldSpatialState *spatial, const AnsiString &name)
        : System(name)
        , spatial_state(spatial)
    {
        SetExecutionPhase(ExecutionPhase::TickTransform);
    }

    void SpatialTransformSystem::Update(float deltaTime)
    {
        (void)deltaTime;
        if (spatial_state)
            spatial_state->UpdateTransforms();
    }
}
