static void RefreshAssetHoverState(GizmoECS *gizmo,
                                  const math::Vector2i &mouse_coord,
                                  const CameraInfo *camera_info,
                                  const ViewportInfo *viewport_info)
{
    if (!gizmo)
        return;

    if (gizmo->root_visible)
        UpdateAssetVisualHover(gizmo, mouse_coord, camera_info, viewport_info);
    else
        SetAssetVisualHighlight(gizmo, false);
}

static bool BeginAssetDragIfNeeded(GizmoECS *gizmo,
                                   const math::Vector2i &mouse_coord,
                                   const CameraInfo *camera_info,
                                   const ViewportInfo *viewport_info,
                                   hgl::ecs::InputSystem *input_system,
                                   bool left_pressed,
                                   bool has_view_context,
                                   const math::Vector3f &prev_pos,
                                   const glm::quat &prev_rot,
                                   const math::Vector3f &prev_scale)
{
    if (!gizmo || !left_pressed || gizmo->asset_drag.dragging)
        return false;

    if (gizmo->root_visible)
        UpdateAssetVisualHover(gizmo, mouse_coord, camera_info, viewport_info);

    const int picked = gizmo->asset_hovered_visual_index;
    auto *items = GetActiveAssetVisualList(gizmo);

    if (gizmo->root_visible && items && picked >= 0 && picked < static_cast<int>(items->size()))
    {
        if (!BeginAssetMouseCapture(gizmo, input_system))
            return true;

        const auto &picked_entry = (*items)[picked];
        gizmo->asset_drag.dragging = true;
        gizmo->asset_drag.mode = gizmo->current_mode;
        gizmo->asset_drag.pick_index = picked;
        gizmo->asset_drag.pick_group = picked_entry.group_id;
        gizmo->asset_drag.pick_shape = picked_entry.shape;
        gizmo->asset_drag.pick_plane_normal_axis = GetScalePlaneNormalAxisFromEntry(picked_entry);
        gizmo->asset_drag.start_mouse = mouse_coord;
        gizmo->asset_drag.start_position = prev_pos;
        gizmo->asset_drag.start_rotation = prev_rot;
        gizmo->asset_drag.start_scale = prev_scale;

        auto &channel = GetAssetChannelState(gizmo, gizmo->asset_drag.mode);
        channel.pick_index = gizmo->asset_drag.pick_index;
        channel.pick_group = gizmo->asset_drag.pick_group;
        channel.pick_plane_normal_axis = gizmo->asset_drag.pick_plane_normal_axis;
        channel.pick_shape = gizmo->asset_drag.pick_shape;
        ApplyAssetVisualHighlightByIndex(gizmo, picked);
    }
    else if (!has_view_context)
    {
        // Keep deterministic headless/no-camera behavior used by smoke tests.
        if (!BeginAssetMouseCapture(gizmo, input_system))
            return true;

        gizmo->asset_drag.dragging = true;
        gizmo->asset_drag.mode = gizmo->current_mode;
        ResetAssetActivePickState(gizmo);
        gizmo->asset_drag.start_mouse = mouse_coord;
        gizmo->asset_drag.start_position = prev_pos;
        gizmo->asset_drag.start_rotation = prev_rot;
        gizmo->asset_drag.start_scale = prev_scale;

        auto &channel = GetAssetChannelState(gizmo, gizmo->asset_drag.mode);
        channel.pick_index = -1;
        channel.pick_group = -1;
        channel.pick_plane_normal_axis = -1;
        channel.pick_shape = GizmoShape::Sphere;
    }

    return false;
}

static void EndAssetDragIfNeeded(GizmoECS *gizmo,
                                 const math::Vector2i &mouse_coord,
                                 const CameraInfo *camera_info,
                                 const ViewportInfo *viewport_info,
                                 bool left_released)
{
    if (!gizmo || !left_released)
        return;

    gizmo->asset_drag.dragging = false;
    ResetAssetActivePickState(gizmo);
    EndAssetMouseCapture(gizmo);
    RefreshAssetHoverState(gizmo, mouse_coord, camera_info, viewport_info);
}

static void RecoverAssetDragIfReleaseMissed(GizmoECS *gizmo, bool left_down, bool left_released)
{
    if (!gizmo)
        return;

    if (gizmo->asset_drag.dragging && !left_down && !left_released)
    {
        // Robust fallback when button-up edge is missed by caller.
        gizmo->asset_drag.dragging = false;
        ResetAssetActivePickState(gizmo);
        EndAssetMouseCapture(gizmo);
    }
}

static void UpdateRotateViewRingFacingToCamera(GizmoECS *gizmo, const CameraInfo *camera_info)
{
    if (!gizmo || !gizmo->RotateChannel().aux_transform || !camera_info)
        return;

    const math::Vector3f forward = glm::normalize(math::Vector3f(camera_info->view[0][2],
                                                                  camera_info->view[1][2],
                                                                  camera_info->view[2][2]));
    const math::Vector3f from = math::AxisVector::X;
    const float dot_value = glm::dot(from, forward);
    glm::quat facing(1.0f, 0.0f, 0.0f, 0.0f);

    if (dot_value < -0.9999f)
    {
        facing = glm::angleAxis(glm::radians(180.0f), math::AxisVector::Y);
    }
    else if (dot_value < 0.9999f)
    {
        const math::Vector3f axis = glm::normalize(glm::cross(from, forward));
        const float angle = std::acos(std::clamp(dot_value, -1.0f, 1.0f));
        facing = glm::angleAxis(angle, axis);
    }

    // White view ring must stay camera-facing in world space.
    // Compensate parent world rotation so local/world rotate modes behave the same.
    glm::quat parent_world_rot(1.0f, 0.0f, 0.0f, 0.0f);
    if (gizmo->RotateChannel().entity)
    {
        auto rotate_entity_transform = gizmo->RotateChannel().entity->GetComponent<hgl::ecs::TransformComponent>();
        if (rotate_entity_transform)
        {
            rotate_entity_transform->UpdateIfDirty();
            parent_world_rot = rotate_entity_transform->GetWorldRotation();
        }
    }

    gizmo->RotateChannel().aux_transform->SetLocalRotation(glm::inverse(parent_world_rot) * facing);
}

static void SyncTargetToRootIfIdle(GizmoECS *gizmo,
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

static void CommitTransformChanges(GizmoECS *gizmo,
                                   const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                                   const math::Vector3f &prev_pos,
                                   const glm::quat &prev_rot,
                                   const math::Vector3f &prev_scale,
                                   const math::Vector3f &cur_effective_scale)
{
    if (!gizmo || !gizmo->root_transform)
        return;

    const math::Vector3f cur_pos = gizmo->root_transform->GetLocalPosition();
    const glm::quat cur_rot = gizmo->root_transform->GetLocalRotation();
    const math::Vector3f cur_scale = target_transform ? cur_effective_scale
                                                      : gizmo->root_transform->GetLocalScale();
    const bool changed = IsTransformChanged(prev_pos, prev_rot, prev_scale,
                                            cur_pos, cur_rot, cur_scale);

    if (target_transform && changed)
        target_transform->SetLocalTRS(cur_pos, cur_rot, cur_scale);

    if (changed && gizmo->on_changed)
    {
        GizmoTransformChange change;
        change.previous_position = prev_pos;
        change.current_position = cur_pos;
        change.previous_rotation = prev_rot;
        change.current_rotation = cur_rot;
        change.previous_scale = prev_scale;
        change.current_scale = cur_scale;
        change.mode = gizmo->current_mode;
        gizmo->on_changed(change);
    }
}

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

    std::shared_ptr<hgl::ecs::TransformComponent> target_transform;
    if (gizmo->target_entity)
        target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();

    const bool has_view_context = (camera_info && viewport_info);
    SyncTargetToRootIfIdle(gizmo, target_transform, has_view_context);

    SyncAssetFixedPixelSizingContext(gizmo, camera_info, viewport_info);
    SyncAssetSubGizmoLocalTransforms(gizmo);

    const math::Vector3f prev_pos = target_transform ? target_transform->GetLocalPosition()
                                                     : gizmo->root_transform->GetLocalPosition();
    const glm::quat prev_rot = target_transform ? target_transform->GetLocalRotation()
                                                : gizmo->root_transform->GetLocalRotation();
    const math::Vector3f prev_scale = target_transform ? target_transform->GetLocalScale()
                                                        : gizmo->root_transform->GetLocalScale();
    math::Vector3f cur_effective_scale = prev_scale;

    if (!gizmo->asset_drag.dragging)
        RefreshAssetHoverState(gizmo, mouse_coord, camera_info, viewport_info);

    if (BeginAssetDragIfNeeded(gizmo,
                               mouse_coord,
                               camera_info,
                               viewport_info,
                               input_system,
                               left_pressed,
                               has_view_context,
                               prev_pos,
                               prev_rot,
                               prev_scale))
        return;

    EndAssetDragIfNeeded(gizmo, mouse_coord, camera_info, viewport_info, left_released);
    RecoverAssetDragIfReleaseMissed(gizmo, left_down, left_released);

    if (gizmo->asset_drag.dragging && left_down)
    {
        auto &active_channel = GetAssetChannelState(gizmo, gizmo->asset_drag.mode);
        gizmo->asset_drag.pick_index = active_channel.pick_index;
        gizmo->asset_drag.pick_group = active_channel.pick_group;
        gizmo->asset_drag.pick_plane_normal_axis = active_channel.pick_plane_normal_axis;
        gizmo->asset_drag.pick_shape = active_channel.pick_shape;

        DispatchActiveAssetDragChannel(gizmo,
                                       mouse_coord,
                                       camera_info,
                                       viewport_info,
                                       target_transform,
                                       has_view_context,
                                       cur_effective_scale);
    }

    UpdateRotateViewRingFacingToCamera(gizmo, camera_info);
    CommitTransformChanges(gizmo,
                           target_transform,
                           prev_pos,
                           prev_rot,
                           prev_scale,
                           cur_effective_scale);
}
