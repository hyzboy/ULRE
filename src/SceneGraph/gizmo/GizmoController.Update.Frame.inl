void GizmoController::SyncTargetToRootIfIdle(GizmoECS *gizmo,
                                             const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                                             bool has_view_context)
{
    if (!gizmo || !gizmo->root_transform || !target_transform || gizmo->asset_drag.dragging)
        return;

    gizmo->root_transform->SetLocalTRS(target_transform->GetLocalPosition(),
                                       target_transform->GetLocalRotation(),
                                       has_view_context ? math::Vector3f(1.0f)
                                                        : target_transform->GetLocalScale());
}

void GizmoController::PrepareUpdateFrameState(GizmoECS *gizmo,
                                              const CameraInfo *camera_info,
                                              const ViewportInfo *viewport_info,
                                              AssetUpdateFrameState &state)
{
    if (!gizmo || !gizmo->root_transform)
        return;

    if (gizmo->target_entity)
        state.target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();

    state.has_view_context = (camera_info && viewport_info);
    GizmoController::SyncTargetToRootIfIdle(gizmo, state.target_transform, state.has_view_context);

    SyncAssetFixedPixelSizingContext(gizmo, camera_info, viewport_info);
    SyncAssetSubGizmoLocalTransforms(gizmo);

    state.prev_pos = state.target_transform ? state.target_transform->GetLocalPosition()
                                            : gizmo->root_transform->GetLocalPosition();
    state.prev_rot = state.target_transform ? state.target_transform->GetLocalRotation()
                                            : gizmo->root_transform->GetLocalRotation();
    state.prev_scale = state.target_transform ? state.target_transform->GetLocalScale()
                                              : gizmo->root_transform->GetLocalScale();
    state.cur_effective_scale = state.prev_scale;
}

bool GizmoController::RunDragUpdateStage(GizmoECS *gizmo,
                                         const math::Vector2i &mouse_coord,
                                         const CameraInfo *camera_info,
                                         const ViewportInfo *viewport_info,
                                         hgl::ecs::InputSystem *input_system,
                                         bool left_down,
                                         bool left_pressed,
                                         bool left_released,
                                         AssetUpdateFrameState &state)
{
    if (!gizmo)
        return false;

    GizmoFrameInput frame_input;
    frame_input.mouse_coord = mouse_coord;
    frame_input.camera_info = camera_info;
    frame_input.viewport_info = viewport_info;
    frame_input.input_system = input_system;
    frame_input.left_down = left_down;
    frame_input.left_pressed = left_pressed;
    frame_input.left_released = left_released;

    GizmoTransformSnapshot snapshot;
    snapshot.position = state.prev_pos;
    snapshot.rotation = state.prev_rot;
    snapshot.scale = state.prev_scale;

    IGizmoChannel *active_channel = gizmo->channel_controller.GetChannelForMode(gizmo->current_mode);
    if (active_channel)
        active_channel->OnPreUpdate(gizmo, frame_input, snapshot);

    if (GizmoController::ShouldRefreshHoverBeforeDrag(gizmo))
        GizmoController::RefreshHoverState(gizmo, mouse_coord, camera_info, viewport_info);

    if (GizmoController::ShouldAttemptBeginDrag(gizmo, left_pressed))
    {
        if (GizmoController::BeginDragIfNeeded(gizmo,
                                               mouse_coord,
                                               camera_info,
                                               viewport_info,
                                               input_system,
                                               state.has_view_context,
                                               state.prev_pos,
                                               state.prev_rot,
                                               state.prev_scale))
            return false;
    }

    if (GizmoController::ShouldEndDragOnRelease(gizmo, left_released))
        GizmoController::EndDragIfNeeded(gizmo, mouse_coord, camera_info, viewport_info);

    GizmoController::RecoverDragLifecycle(gizmo, left_down, left_released);

    if (GizmoController::CanRunDragDispatch(gizmo, left_down))
    {
        GizmoController::SyncActivePickFromChannelForDrag(gizmo);

        // Phase 3: Move drag dispatched directly through MoveGizmoMode.
        if (GizmoController::IsMoveMode(gizmo->asset_drag.mode))
        {
            gizmo->move_mode.ApplyDrag(mouse_coord, camera_info, viewport_info,
                                       gizmo->root_transform);
        }
        else
        {
            bool handled_by_channel = false;
            if (IGizmoChannel *drag_channel = gizmo->channel_controller.GetChannelForMode(gizmo->asset_drag.mode))
            {
                handled_by_channel = drag_channel->DispatchDrag(gizmo,
                                                                mouse_coord,
                                                                camera_info,
                                                                viewport_info,
                                                                state.target_transform,
                                                                state.has_view_context,
                                                                state.cur_effective_scale);
            }

            if (!handled_by_channel)
            {
                DispatchActiveAssetDragChannel(gizmo,
                                               mouse_coord,
                                               camera_info,
                                               viewport_info,
                                               state.target_transform,
                                               state.has_view_context,
                                               state.cur_effective_scale);
            }
        }

        snapshot.scale = state.cur_effective_scale;
    }

    if (active_channel)
        active_channel->OnPostUpdate(gizmo, frame_input, snapshot);

    return true;
}
