void GizmoController::UpdateRotateViewRingFacingToCamera(GizmoECS *gizmo, const CameraInfo *camera_info)
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

    // White view ring must stay camera-facing in world space.
    // Compensate parent world rotation so local/world rotate modes behave the same.
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

void GizmoController::CommitTransformChanges(GizmoECS *gizmo,
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

void GizmoController::FinalizeUpdateFrameState(GizmoECS *gizmo,
                                               const CameraInfo *camera_info,
                                               AssetUpdateFrameState &state)
{
    if (!gizmo)
        return;

    GizmoController::UpdateRotateViewRingFacingToCamera(gizmo, camera_info);
    GizmoController::CommitTransformChanges(gizmo,
                                            state.target_transform,
                                            state.prev_pos,
                                            state.prev_rot,
                                            state.prev_scale,
                                            state.cur_effective_scale);
}
