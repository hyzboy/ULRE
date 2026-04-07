// RotateGizmoMode — interactive (hover + drag) methods.
// Included as part of GizmoUnified.cpp (single-TU pattern), so static helpers
// from GizmoUnified.AssetVisual.inl and GizmoUnified.AssetChannels.inl are in scope.

void RotateGizmoMode::UpdateHover(const GizmoFrameInput &input,
                                   const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform)
{
    const int best_index = PickBestAssetVisualIndex(primitives, root_transform, input);
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
        {
            auto *yellow_mi = GetGizmoMI3D(GizmoColor::Yellow);
            p.primitive->SetMIIDOverride(yellow_mi ? yellow_mi->GetMIID() : -1);
        }
        else
            p.primitive->SetMIIDOverride(p.base_mi_id);
    }
}

bool RotateGizmoMode::TryBeginDrag(const GizmoFrameInput &input,
                                    const GizmoPrevTransform &prev,
                                    GizmoMode current_mode,
                                    bool root_visible)
{
    if (drag.active)
        return false;

    const bool has_view_context = (input.camera_info && input.viewport_info);
    const bool can_pick = (root_visible
                           && hovered_index >= 0
                           && hovered_index < static_cast<int>(primitives.size()));

    if (!can_pick && has_view_context)
        return false;  // Nothing to drag

    // Attempt mouse capture.
    if (input.input_system)
    {
        if (drag.mouse_captured)
        {
            if (drag.capture_input_sys == input.input_system)
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
            if (!input.input_system->BeginMouseCapture(this))
                return true;  // Capture failed → abort begin-drag

            drag.mouse_captured = true;
            drag.capture_input_sys = input.input_system;
        }
    }

    // Snapshot transform at drag-begin.
    drag.active         = true;
    drag.mode           = current_mode;
    drag.start_mouse    = input.mouse_coord;
    drag.start_position = prev.pos;
    drag.start_rotation = prev.rot;
    drag.start_scale    = prev.scale;

    if (!can_pick)
        drag.pick = GizmoPickState{};
    // When can_pick: drag.pick already set by the preceding UpdateHover call.

    return false;
}

void RotateGizmoMode::ApplyDrag(const GizmoFrameInput &input,
                                 const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform)
{
    const math::Vector2i &mouse = input.mouse_coord;
    const CameraInfo    *cam   = input.camera_info;
    const ViewportInfo  *vp    = input.viewport_info;
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

    const bool local_space = IsRotateMode(drag.mode)
                          && IsLocalMode(drag.mode);

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

        // Sign correction: compute_delta_angle() is positive for CW screen drag
        // (screen Y is down, so cross_z > 0 = CW). glm::angleAxis(+θ, axis) is
        // CCW when viewed from the positive end of axis (right-hand rule).
        // (view[0][2], view[1][2], view[2][2]) = row 2 of view = camera forward
        // (toward scene, AWAY from viewer). When dot(axis, cam_forward) < 0 the
        // axis tip faces toward the viewer, so CW drag → CCW rotation → need flip.
        float signed_angle = delta_angle;
        if (cam)
        {
            const glm::vec3 cam_forward = glm::normalize(math::Vector3f(
                cam->view[0][2], cam->view[1][2], cam->view[2][2]));
            if (glm::dot(glm::normalize(axis), cam_forward) < 0.0f)
                signed_angle = -delta_angle;
        }

        const glm::quat dq = glm::angleAxis(signed_angle, glm::normalize(axis));
        root_transform->SetLocalRotation(glm::normalize(dq * drag.start_rotation));
        return;
    }

    // ─── No pick (headless / degenerate): screen-space tumble ────────────
    if (IsRotateMode(drag.mode) && IsWorldMode(drag.mode))
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

    drag = GizmoDragState{};
    hovered_index = -1;
}

void RotateGizmoMode::RecoverIfOrphaned(bool left_down)
{
    if (drag.active && !left_down)
        EndDrag();
}
