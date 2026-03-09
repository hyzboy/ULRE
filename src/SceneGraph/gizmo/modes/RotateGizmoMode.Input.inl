// RotateGizmoMode — interactive (hover + drag) methods.
// Included as part of GizmoUnified.cpp (single-TU pattern), so static helpers
// from GizmoUnified.AssetVisual.inl and GizmoUnified.AssetChannels.inl are in scope.

void RotateGizmoMode::UpdateHover(const math::Vector2i &mouse,
                                   const CameraInfo *cam,
                                   const ViewportInfo *vp,
                                   const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform)
{
    const int best_index = PickBestAssetVisualIndex(primitives, root_transform, mouse, cam, vp);
    hovered_index = best_index;

    // Update pre-computed pick state so TryBeginDrag reads it instantly.
    if (best_index >= 0 && best_index < static_cast<int>(primitives.size()))
    {
        const auto &entry = primitives[best_index];
        drag.pick.pick_index             = best_index;
        drag.pick.pick_group             = entry.group_id;
        drag.pick.pick_shape             = entry.shape;
        drag.pick.pick_plane_normal_axis = -1; // not used for rotate
    }
    else
    {
        drag.pick = GizmoPickState{};
    }

    // Apply visual highlight directly to our own primitives.
    const int best_group = (best_index >= 0 && best_index < static_cast<int>(primitives.size()))
                         ? primitives[best_index].group_id : -1;

    for (size_t i = 0; i < primitives.size(); ++i)
    {
        auto &p = primitives[i];
        if (!p.primitive)
            continue;

        const bool in_group = (best_group >= 0 && p.group_id == best_group);
        if (static_cast<int>(i) == best_index || in_group)
            p.primitive->SetOverrideMaterial(GetGizmoMI3D(GizmoColor::Yellow));
        else
            p.primitive->SetOverrideMaterial(p.base_material);
    }
}

bool RotateGizmoMode::TryBeginDrag(const math::Vector2i &mouse,
                                    const CameraInfo *cam,
                                    const ViewportInfo *vp,
                                    hgl::ecs::InputSystem *input_sys,
                                    bool has_view_context,
                                    const math::Vector3f &prev_pos,
                                    const glm::quat &prev_rot,
                                    const math::Vector3f &prev_scale,
                                    GizmoMode current_mode,
                                    bool root_visible)
{
    if (drag.active)
        return false;

    const bool can_pick = (root_visible
                           && hovered_index >= 0
                           && hovered_index < static_cast<int>(primitives.size()));

    if (!can_pick && has_view_context)
        return false;  // Nothing to drag

    // Attempt mouse capture.
    if (input_sys)
    {
        if (drag.mouse_captured)
        {
            if (drag.capture_input_sys == input_sys)
            {
                // Already captured — proceed.
            }
            else
            {
                drag.capture_input_sys->EndMouseCapture(this);
                drag.mouse_captured = false;
                drag.capture_input_sys = nullptr;
            }
        }

        if (!drag.mouse_captured)
        {
            if (!input_sys->BeginMouseCapture(this))
                return true;  // Capture failed → abort begin-drag

            drag.mouse_captured = true;
            drag.capture_input_sys = input_sys;
        }
    }

    // Snapshot transform at drag-begin.
    drag.active         = true;
    drag.mode           = current_mode;
    drag.start_mouse    = mouse;
    drag.start_position = prev_pos;
    drag.start_rotation = prev_rot;
    drag.start_scale    = prev_scale;

    if (!can_pick)
        drag.pick = GizmoPickState{};
    // When can_pick: drag.pick already set by the preceding UpdateHover call.

    return false;
}

void RotateGizmoMode::ApplyDrag(const math::Vector2i &mouse,
                                 const CameraInfo *cam,
                                 const ViewportInfo *vp,
                                 const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform)
{
    if (!root_transform || !drag.active)
        return;

    constexpr float kRotateSensitivity = 0.005f;

    const float dx = static_cast<float>(mouse.x - drag.start_mouse.x);
    const float dy = static_cast<float>(mouse.y - drag.start_mouse.y);

    math::Vector3f camera_right = math::AxisVector::X;
    math::Vector3f camera_up    = math::AxisVector::Y;
    if (cam)
    {
        camera_right = glm::normalize(math::Vector3f(cam->view[0][0], cam->view[1][0], cam->view[2][0]));
        camera_up    = glm::normalize(math::Vector3f(cam->view[0][1], cam->view[1][1], cam->view[2][1]));
    }

    const bool local_space = GizmoController::IsRotateMode(drag.mode)
                          && GizmoController::IsLocalMode(drag.mode);

    // Helper: axis in world space from axis_index (0=X, 1=Y, 2=Z), respecting local mode.
    auto axis_from_index = [&](int axis_index) -> glm::vec3
    {
        glm::vec3 axis = math::GetAxisVector(math::AXIS(axis_index));
        if (local_space)
            axis = drag.start_rotation * axis;
        return glm::normalize(axis);
    };

    // ─── Compute rotation delta angle ────────────────────────────────────
    // Use the arc-tangent of the mouse movement around the gizmo centre on screen.
    auto compute_delta_angle = [&]() -> float
    {
        if (!cam || !vp)
            return (-dx - dy) * kRotateSensitivity;

        const math::Vector2u vp_size = vp->GetViewport();
        if (vp_size.x == 0 || vp_size.y == 0)
            return (-dx - dy) * kRotateSensitivity;

        const math::Vector2i center_px = WorldPositionToScreen(drag.start_position, cam, vp_size);
        const glm::vec2 c(static_cast<float>(center_px.x), static_cast<float>(center_px.y));
        const glm::vec2 v0(static_cast<float>(drag.start_mouse.x) - c.x,
                           static_cast<float>(drag.start_mouse.y) - c.y);
        const glm::vec2 v1(static_cast<float>(mouse.x) - c.x,
                           static_cast<float>(mouse.y) - c.y);

        if (glm::length(v0) < 4.0f || glm::length(v1) < 4.0f)
            return (-dx - dy) * kRotateSensitivity;

        const float cross_z = v0.x * v1.y - v0.y * v1.x;
        return std::atan2(cross_z, glm::dot(v0, v1));
    };

    const float delta_angle = compute_delta_angle();

    // ─── Pick-group present: axis drag ───────────────────────────────────
    if (drag.pick.pick_group >= 0)
    {
        glm::vec3 axis = math::AxisVector::Z;

        if (drag.pick.pick_group < 3)
        {
            axis = axis_from_index(drag.pick.pick_group);
        }
        else if (cam)
        {
            // View ring (group 3) — rotate around camera forward axis.
            axis = glm::normalize(math::Vector3f(cam->view[0][2],
                                                 cam->view[1][2],
                                                 cam->view[2][2]));
        }

        const glm::quat dq = glm::angleAxis(delta_angle, glm::normalize(axis));
        root_transform->SetLocalRotation(glm::normalize(dq * drag.start_rotation));
        return;
    }

    // ─── No pick (headless / degenerate): screen-space tumble ────────────
    if (GizmoController::IsRotateMode(drag.mode) && GizmoController::IsWorldMode(drag.mode))
    {
        const glm::quat yaw   = glm::angleAxis(-dx * kRotateSensitivity, math::AxisVector::Y);
        const glm::quat pitch = glm::angleAxis(-dy * kRotateSensitivity, camera_right);
        root_transform->SetLocalRotation(glm::normalize(yaw * pitch * drag.start_rotation));
    }
    else
    {
        const glm::vec3 local_x = drag.start_rotation * camera_right;
        const glm::vec3 local_y = drag.start_rotation * camera_up;
        const glm::quat yaw_local   = glm::angleAxis(-dx * kRotateSensitivity, local_y);
        const glm::quat pitch_local = glm::angleAxis(-dy * kRotateSensitivity, local_x);
        root_transform->SetLocalRotation(glm::normalize(yaw_local * pitch_local * drag.start_rotation));
    }
}

void RotateGizmoMode::EndDrag()
{
    if (drag.mouse_captured && drag.capture_input_sys)
        drag.capture_input_sys->EndMouseCapture(this);

    drag = RotateDragState{};
    hovered_index = -1;
}

void RotateGizmoMode::RecoverIfOrphaned(bool left_down)
{
    if (drag.active && !left_down)
        EndDrag();
}
