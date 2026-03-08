void MoveGizmoChannel::RefreshHoverState(GizmoECS *gizmo,
                                         const math::Vector2i &mouse_coord,
                                         const CameraInfo *camera_info,
                                         const ViewportInfo *viewport_info,
                                         bool &handled)
{
    handled = false;

    if (!gizmo || !SupportsMode(gizmo->current_mode))
        return;

    handled = true;
    if (gizmo->root_visible)
        UpdateAssetVisualHover(gizmo, mouse_coord, camera_info, viewport_info);
    else
        SetAssetVisualHighlight(gizmo, false);
}

bool MoveGizmoChannel::BeginDragIfNeeded(GizmoECS *gizmo,
                                         const math::Vector2i &mouse_coord,
                                         const CameraInfo *camera_info,
                                         const ViewportInfo *viewport_info,
                                         hgl::ecs::InputSystem *input_system,
                                         bool has_view_context,
                                         const math::Vector3f &prev_pos,
                                         const glm::quat &prev_rot,
                                         const math::Vector3f &prev_scale,
                                         bool &handled)
{
    handled = false;

    if (!gizmo || !SupportsMode(gizmo->current_mode) || gizmo->asset_drag.dragging)
        return false;

    handled = true;

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
        return false;
    }

    if (!has_view_context)
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

void MoveGizmoChannel::EndDragIfNeeded(GizmoECS *gizmo,
                                       const math::Vector2i &mouse_coord,
                                       const CameraInfo *camera_info,
                                       const ViewportInfo *viewport_info,
                                       bool &handled)
{
    handled = false;

    if (!gizmo || !SupportsMode(gizmo->asset_drag.mode))
        return;

    handled = true;
    GizmoController::StopDragCommonState(gizmo);
    GizmoController::RefreshHoverState(gizmo, mouse_coord, camera_info, viewport_info);
}

void MoveGizmoChannel::RecoverDragIfReleaseMissed(GizmoECS *gizmo, bool &handled)
{
    handled = false;

    if (!gizmo || !SupportsMode(gizmo->asset_drag.mode))
        return;

    handled = true;
    GizmoController::StopDragCommonState(gizmo);
}

bool MoveGizmoChannel::DispatchDrag(GizmoECS *gizmo,
                                    const math::Vector2i &mouse_coord,
                                    const CameraInfo *camera_info,
                                    const ViewportInfo *viewport_info,
                                    const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                                    bool has_view_context,
                                    math::Vector3f &cur_effective_scale)
{
    (void)target_transform;
    (void)has_view_context;
    (void)cur_effective_scale;

    if (!gizmo)
        return false;

    if (!SupportsMode(gizmo->asset_drag.mode))
        return false;

    const float dx = static_cast<float>(mouse_coord.x - gizmo->asset_drag.start_mouse.x);
    const float dy = static_cast<float>(mouse_coord.y - gizmo->asset_drag.start_mouse.y);

    constexpr float kMoveSensitivity = 0.01f;

    math::Vector3f camera_right = math::AxisVector::X;
    math::Vector3f camera_up = math::AxisVector::Y;
    if (camera_info)
    {
        camera_right = glm::normalize(math::Vector3f(camera_info->view[0][0], camera_info->view[1][0], camera_info->view[2][0]));
        camera_up = glm::normalize(math::Vector3f(camera_info->view[0][1], camera_info->view[1][1], camera_info->view[2][1]));
    }

    ApplyAssetMoveDragChannel(gizmo,
                              mouse_coord,
                              camera_info,
                              viewport_info,
                              camera_right,
                              camera_up,
                              dx,
                              dy,
                              kMoveSensitivity);
    return true;
}
