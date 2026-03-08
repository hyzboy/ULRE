#include "GizmoController.Update.inl"

void UpdateTransformGizmo(GizmoECS *gizmo,
                          const math::Vector2i &mouse_coord,
                          const CameraInfo *camera_info,
                          const ViewportInfo *viewport_info,
                          hgl::ecs::InputSystem *input_system,
                          bool left_down,
                          bool left_pressed,
                          bool left_released)
{
    if (!gizmo || !gizmo->root_transform)
        return;

    AssetUpdateFrameState state;
    GizmoController::PrepareUpdateFrameState(gizmo, camera_info, viewport_info, state);

    if (!GizmoController::RunDragUpdateStage(gizmo,
                                             mouse_coord,
                                             camera_info,
                                             viewport_info,
                                             input_system,
                                             left_down,
                                             left_pressed,
                                             left_released,
                                             state))
        return;

    GizmoController::FinalizeUpdateFrameState(gizmo, camera_info, state);
}
