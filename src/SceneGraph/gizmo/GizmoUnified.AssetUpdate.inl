// Phase 5: GizmoController removed — direct-dispatch pattern.
// CommitTransformChanges and UpdateRotateViewRingFacingToCamera are promoted here
// from the deleted GizmoController.Update.Commit.inl.

static void UpdateRotateViewRingFacingToCamera(GizmoECS *gizmo, const CameraInfo *camera_info)
{
    if (!gizmo || !gizmo->rotate_mode.aux_transform || !camera_info)
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

    glm::quat parent_world_rot(1.0f, 0.0f, 0.0f, 0.0f);
    if (gizmo->rotate_mode.entity)
    {
        auto rotate_entity_transform = gizmo->rotate_mode.entity->GetComponent<hgl::ecs::TransformComponent>();
        if (rotate_entity_transform)
        {
            rotate_entity_transform->UpdateIfDirty();
            parent_world_rot = rotate_entity_transform->GetWorldRotation();
        }
    }

    gizmo->rotate_mode.aux_transform->SetLocalRotation(glm::inverse(parent_world_rot) * facing);
}

struct GizmoPrevTransform
{
    math::Vector3f  pos;
    glm::quat       rot{1.0f, 0.0f, 0.0f, 0.0f};
    math::Vector3f  scale{1.0f, 1.0f, 1.0f};
};

static void CommitTransformChanges(GizmoECS *gizmo,
                                   const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                                   const GizmoPrevTransform &prev,
                                   const math::Vector3f &cur_effective_scale)
{
    if (!gizmo || !gizmo->root_transform)
        return;

    const math::Vector3f cur_pos = gizmo->root_transform->GetLocalPosition();
    const glm::quat cur_rot = gizmo->root_transform->GetLocalRotation();
    const math::Vector3f cur_scale = target_transform ? cur_effective_scale
                                                      : gizmo->root_transform->GetLocalScale();
    const bool changed = IsTransformChanged(prev.pos, prev.rot, prev.scale,
                                            cur_pos, cur_rot, cur_scale);

    if (target_transform && changed)
        target_transform->SetLocalTRS(cur_pos, cur_rot, cur_scale);

    if (changed && gizmo->on_changed)
    {
        GizmoTransformChange change;
        change.previous_position = prev.pos;
        change.current_position  = cur_pos;
        change.previous_rotation = prev.rot;
        change.current_rotation  = cur_rot;
        change.previous_scale    = prev.scale;
        change.current_scale     = cur_scale;
        change.mode              = gizmo->current_mode;
        gizmo->on_changed(change);
    }
}

template <typename Mode>
static void DispatchGizmoMode(Mode &mode, GizmoECS *gizmo,
                               const GizmoFrameInput &input,
                               const GizmoPrevTransform &prev)
{
    if (!mode.IsDragging())
        mode.UpdateHover(input.mouse_coord, input.camera_info, input.viewport_info, gizmo->root_transform);

    const bool has_view = (input.camera_info && input.viewport_info);
    if (input.left_pressed && !mode.IsDragging())
        mode.TryBeginDrag(input.mouse_coord, input.camera_info, input.viewport_info, input.input_system,
                          has_view, prev.pos, prev.rot, prev.scale,
                          gizmo->current_mode, gizmo->root_visible);

    if (input.left_released && mode.IsDragging())
        mode.EndDrag();

    if (!input.left_down && mode.IsDragging())
        mode.RecoverIfOrphaned(input.left_down);
}

void UpdateTransformGizmo(GizmoECS *gizmo, const GizmoFrameInput &input)
{
    if (!gizmo || !gizmo->root_transform)
        return;

    std::shared_ptr<hgl::ecs::TransformComponent> target_transform;
    if (gizmo->target_entity)
        target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();

    const bool has_view = (input.camera_info && input.viewport_info);

    // 1. Sync target → root when idle
    if (!IsAnyModeDragging(gizmo) && target_transform)
    {
        gizmo->root_transform->SetLocalTRS(
            target_transform->GetLocalPosition(),
            target_transform->GetLocalRotation(),
            has_view ? math::Vector3f(1.0f) : target_transform->GetLocalScale());
    }

    SyncAssetFixedPixelSizingContext(gizmo, input.camera_info, input.viewport_info);
    SyncAssetSubGizmoLocalTransforms(gizmo);

    GizmoPrevTransform prev;
    prev.pos   = target_transform ? target_transform->GetLocalPosition()
                                  : gizmo->root_transform->GetLocalPosition();
    prev.rot   = target_transform ? target_transform->GetLocalRotation()
                                  : gizmo->root_transform->GetLocalRotation();
    prev.scale = target_transform ? target_transform->GetLocalScale()
                                  : gizmo->root_transform->GetLocalScale();
    math::Vector3f cur_effective_scale = prev.scale;

    // 2. Dispatch to active mode
    switch (gizmo->current_mode)
    {
    case GizmoMode::MoveWorld:
    case GizmoMode::MoveLocal:
        DispatchGizmoMode(gizmo->move_mode, gizmo, input, prev);
        if (gizmo->move_mode.IsDragging() && input.left_down)
            gizmo->move_mode.ApplyDrag(input.mouse_coord, input.camera_info, input.viewport_info,
                                       gizmo->root_transform);
        break;

    case GizmoMode::RotateWorld:
    case GizmoMode::RotateLocal:
        DispatchGizmoMode(gizmo->rotate_mode, gizmo, input, prev);
        if (gizmo->rotate_mode.IsDragging() && input.left_down)
            gizmo->rotate_mode.ApplyDrag(input.mouse_coord, input.camera_info, input.viewport_info,
                                         gizmo->root_transform);
        break;

    case GizmoMode::ScaleLocal:
        DispatchGizmoMode(gizmo->scale_mode, gizmo, input, prev);
        if (gizmo->scale_mode.IsDragging() && input.left_down)
            gizmo->scale_mode.ApplyDrag(input.mouse_coord, input.camera_info, input.viewport_info,
                                        gizmo->allow_negative_scale,
                                        target_transform, has_view,
                                        gizmo->root_transform, cur_effective_scale);
        break;
    }

    // 3. Post-frame
    UpdateRotateViewRingFacingToCamera(gizmo, input.camera_info);
    CommitTransformChanges(gizmo, target_transform, prev, cur_effective_scale);
}
