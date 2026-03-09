void GizmoController::RefreshHoverState(GizmoECS *gizmo,
                                        const math::Vector2i &mouse_coord,
                                        const CameraInfo *camera_info,
                                        const ViewportInfo *viewport_info)
{
    if (!gizmo)
        return;

    // Phase 3: Move bypasses the channel — call mode directly.
    if (GizmoController::IsMoveMode(gizmo->current_mode))
    {
        if (gizmo->root_visible)
            gizmo->move_mode.UpdateHover(mouse_coord, camera_info, viewport_info, gizmo->root_transform);
        else
            SetAssetVisualHighlight(gizmo, false);
        return;
    }

    if (IGizmoChannel *channel = gizmo->channel_controller.GetChannelForMode(gizmo->current_mode))
    {
        bool handled = false;
        channel->RefreshHoverState(gizmo, mouse_coord, camera_info, viewport_info, handled);
        if (handled)
            return;
    }

    if (gizmo->root_visible)
        UpdateAssetVisualHover(gizmo, mouse_coord, camera_info, viewport_info);
    else
        SetAssetVisualHighlight(gizmo, false);
}

void GizmoController::StartDragCommonState(GizmoECS *gizmo,
                                           const math::Vector2i &mouse_coord,
                                           const math::Vector3f &prev_pos,
                                           const glm::quat &prev_rot,
                                           const math::Vector3f &prev_scale)
{
    if (!gizmo)
        return;

    gizmo->asset_drag.dragging = true;
    gizmo->asset_drag.mode = gizmo->current_mode;
    gizmo->asset_drag.start_mouse = mouse_coord;
    gizmo->asset_drag.start_position = prev_pos;
    gizmo->asset_drag.start_rotation = prev_rot;
    gizmo->asset_drag.start_scale = prev_scale;
}

void GizmoController::StopDragCommonState(GizmoECS *gizmo)
{
    if (!gizmo)
        return;

    gizmo->asset_drag.dragging = false;
    ResetAssetActivePickState(gizmo);
    EndAssetMouseCapture(gizmo);
}

bool GizmoController::BeginDragIfNeeded(GizmoECS *gizmo,
                                        const math::Vector2i &mouse_coord,
                                        const CameraInfo *camera_info,
                                        const ViewportInfo *viewport_info,
                                        hgl::ecs::InputSystem *input_system,
                                        bool has_view_context,
                                        const math::Vector3f &prev_pos,
                                        const glm::quat &prev_rot,
                                        const math::Vector3f &prev_scale)
{
    if (!gizmo || gizmo->asset_drag.dragging)
        return false;

    // Phase 3: Move bypasses the channel — delegate entirely to move_mode.
    if (GizmoController::IsMoveMode(gizmo->current_mode))
    {
        const bool abort = gizmo->move_mode.TryBeginDrag(
            mouse_coord, camera_info, viewport_info,
            input_system, has_view_context,
            prev_pos, prev_rot, prev_scale,
            gizmo->current_mode, gizmo->root_visible);

        // Sync shared drag metadata so EndDrag/Recover can identify the active mode.
        if (gizmo->move_mode.drag.active)
        {
            gizmo->asset_drag.dragging = true;
            gizmo->asset_drag.mode     = gizmo->current_mode;
            // Also sync pick into active-pick so channel helpers still work for Move.
            const auto &p = gizmo->move_mode.drag.pick;
            SetAssetActivePickState(gizmo, p.pick_index, p.pick_group,
                                    p.pick_plane_normal_axis, p.pick_shape);
        }
        return abort;
    }

    if (IGizmoChannel *channel = gizmo->channel_controller.GetChannelForMode(gizmo->current_mode))
    {
        bool handled = false;
        const bool should_abort = channel->BeginDragIfNeeded(gizmo,
                                                             mouse_coord,
                                                             camera_info,
                                                             viewport_info,
                                                             input_system,
                                                             has_view_context,
                                                             prev_pos,
                                                             prev_rot,
                                                             prev_scale,
                                                             handled);
        if (should_abort)
            return true;

        if (handled)
            return false;
    }

    if (gizmo->root_visible)
        UpdateAssetVisualHover(gizmo, mouse_coord, camera_info, viewport_info);

    const int picked = gizmo->asset_hovered_visual_index;
    auto *items = GetActiveAssetVisualList(gizmo);

    if (gizmo->root_visible && items && picked >= 0 && picked < static_cast<int>(items->size()))
    {
        if (!BeginAssetMouseCapture(gizmo, input_system))
            return true;

        const auto &picked_entry = (*items)[picked];
        GizmoController::StartDragCommonState(gizmo, mouse_coord, prev_pos, prev_rot, prev_scale);
        SetAssetActivePickState(gizmo,
                                picked,
                                picked_entry.group_id,
                                GetScalePlaneNormalAxisFromEntry(picked_entry),
                                picked_entry.shape);

        SyncAssetChannelPickFromActive(gizmo, gizmo->asset_drag.mode);
        ApplyAssetVisualHighlightByIndex(gizmo, picked);
    }
    else if (!has_view_context)
    {
        // Keep deterministic headless/no-camera behavior used by smoke tests.
        if (!BeginAssetMouseCapture(gizmo, input_system))
            return true;

        GizmoController::StartDragCommonState(gizmo, mouse_coord, prev_pos, prev_rot, prev_scale);
        ResetAssetActivePickState(gizmo);
        SyncAssetChannelPickFromActive(gizmo, gizmo->asset_drag.mode);
    }

    return false;
}

void GizmoController::EndDragIfNeeded(GizmoECS *gizmo,
                                      const math::Vector2i &mouse_coord,
                                      const CameraInfo *camera_info,
                                      const ViewportInfo *viewport_info)
{
    if (!gizmo)
        return;

    // Phase 3: Move bypasses the channel — mode handles its own cleanup.
    if (GizmoController::IsMoveMode(gizmo->asset_drag.mode))
    {
        gizmo->move_mode.EndDrag();
        gizmo->asset_drag.dragging = false;
        ResetAssetActivePickState(gizmo);
        GizmoController::RefreshHoverState(gizmo, mouse_coord, camera_info, viewport_info);
        return;
    }

    if (IGizmoChannel *channel = gizmo->channel_controller.GetChannelForMode(gizmo->asset_drag.mode))
    {
        bool handled = false;
        channel->EndDragIfNeeded(gizmo, mouse_coord, camera_info, viewport_info, handled);
        if (handled)
            return;
    }

    GizmoController::StopDragCommonState(gizmo);
    GizmoController::RefreshHoverState(gizmo, mouse_coord, camera_info, viewport_info);
}

void GizmoController::RecoverDragIfReleaseMissed(GizmoECS *gizmo)
{
    if (!gizmo)
        return;

    // Phase 3: Move bypasses the channel.
    if (GizmoController::IsMoveMode(gizmo->asset_drag.mode))
    {
        gizmo->move_mode.EndDrag();
        gizmo->asset_drag.dragging = false;
        ResetAssetActivePickState(gizmo);
        return;
    }

    if (IGizmoChannel *channel = gizmo->channel_controller.GetChannelForMode(gizmo->asset_drag.mode))
    {
        bool handled = false;
        channel->RecoverDragIfReleaseMissed(gizmo, handled);
        if (handled)
            return;
    }

    // Robust fallback when button-up edge is missed by caller.
    GizmoController::StopDragCommonState(gizmo);
}

bool GizmoController::ShouldRefreshHoverBeforeDrag(const GizmoECS *gizmo)
{
    return gizmo && !gizmo->asset_drag.dragging;
}

bool GizmoController::CanRunDragDispatch(const GizmoECS *gizmo, bool left_down)
{
    return gizmo && gizmo->asset_drag.dragging && left_down;
}

bool GizmoController::ShouldAttemptBeginDrag(const GizmoECS *gizmo, bool left_pressed)
{
    return gizmo && left_pressed && !gizmo->asset_drag.dragging;
}

bool GizmoController::ShouldEndDragOnRelease(const GizmoECS *gizmo, bool left_released)
{
    return gizmo && left_released;
}

void GizmoController::RecoverDragLifecycle(GizmoECS *gizmo, bool left_down, bool left_released)
{
    if (!gizmo)
        return;

    if (gizmo->asset_drag.dragging && !left_down && !left_released)
        GizmoController::RecoverDragIfReleaseMissed(gizmo);
}

void GizmoController::SyncActivePickFromChannelForDrag(GizmoECS *gizmo)
{
    if (!gizmo)
        return;

    SyncAssetActivePickFromChannel(gizmo, gizmo->asset_drag.mode);
}
