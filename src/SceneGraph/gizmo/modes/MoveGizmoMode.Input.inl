// MoveGizmoMode — interactive (hover + drag) methods.
// Included as part of GizmoUnified.cpp (single-TU pattern), so static helpers
// from GizmoUnified.AssetVisual.inl and GizmoUnified.AssetChannels.inl are in scope.

void MoveGizmoMode::UpdateHover(const GizmoFrameInput &input,
                                 const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform)
{
    const int best_index = PickBestAssetVisualIndex(primitives, root_transform, input);
    hovered_index = best_index;

    // Update pre-computed pick state so TryBeginDrag can read it instantly.
    if (best_index >= 0 && best_index < static_cast<int>(primitives.size()))
    {
        const auto &entry = primitives[best_index];
        drag.pick.pick_index  = best_index;
        drag.pick.pick_group  = entry.group_id;
        drag.pick.pick_shape  = entry.shape;
        drag.pick.pick_plane_normal_axis = GetScalePlaneNormalAxisFromEntry(entry);
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
            p.primitive->SetOverrideBindingInstance(GetGizmoMI3D(GizmoColor::Yellow));
        else
            p.primitive->SetOverrideBindingInstance(p.base_material);
    }
}

bool MoveGizmoMode::TryBeginDrag(const GizmoFrameInput &input,
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
                // Stale capture; release and re-acquire.
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
    {
        // Headless/no-camera path: no pick, centre drag.
        drag.pick = GizmoPickState{};
    }
    // When can_pick: drag.pick is already accurate from the preceding UpdateHover call.

    return false;
}

void MoveGizmoMode::ApplyDrag(const GizmoFrameInput &input,
                               const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform)
{
    const math::Vector2i &mouse = input.mouse_coord;
    const CameraInfo    *cam   = input.camera_info;
    const ViewportInfo  *vp    = input.viewport_info;
    if (!root_transform || !drag.active)
        return;

    constexpr float kMoveSensitivity = 0.01f;

    const bool local_space = IsMoveMode(drag.mode)
                           && IsLocalMode(drag.mode);

    // Helper: world-space axis for axis_index (0=X,1=Y,2=Z).
    auto axis_from_index = [&](int axis_index) -> glm::vec3
    {
        glm::vec3 axis = math::GetAxisVector(math::AXIS(axis_index));
        if (local_space)
            axis = drag.start_rotation * axis;
        return glm::normalize(axis);
    };

    // ─── Axis drag (Cylinder / Cone pick) ────────────────────────────────
    if (drag.pick.pick_group >= 0 && drag.pick.pick_group < 3)
    {
        const glm::vec3 move_axis = axis_from_index(drag.pick.pick_group);

        float delta_world = 0.0f;

        if (cam && vp)
        {
            const math::Vector2u vp_size = vp->GetViewport();
            if (vp_size.x > 0 && vp_size.y > 0)
            {
                const math::Vector3f p0 = drag.start_position;
                const math::Vector3f p1 = p0 + move_axis * (GIZMO_ARROW_LENGTH * kAssetVisualScale);
                const math::Vector2i s0 = WorldPositionToScreen(p0, cam, vp_size);
                const math::Vector2i s1 = WorldPositionToScreen(p1, cam, vp_size);

                glm::vec2 dir(static_cast<float>(s1.x - s0.x),
                              static_cast<float>(s1.y - s0.y));
                const float len = glm::length(dir);
                if (len >= 1e-4f)
                {
                    dir /= len;
                    const glm::vec2 md(static_cast<float>(mouse.x - drag.start_mouse.x),
                                       static_cast<float>(mouse.y - drag.start_mouse.y));
                    delta_world = glm::dot(md, dir);
                }
            }

            const float wupp = root_transform->ComputeWorldUnitsPerPixel(cam, vp);
            delta_world *= (wupp > 0.0f) ? wupp : kMoveSensitivity;
        }
        else
        {
            delta_world *= kMoveSensitivity;
        }

        root_transform->SetLocalPosition(drag.start_position + move_axis * delta_world);
        return;
    }

    // ─── Plane drag (Square pick) ─────────────────────────────────────────
    if (drag.pick.pick_shape == GizmoShape::Square
        && drag.pick.pick_plane_normal_axis >= 0
        && cam && vp)
    {
        int u_axis = 0, v_axis = 1;
        AssetPlaneAxesFromNormal(drag.pick.pick_plane_normal_axis, u_axis, v_axis);

        const glm::vec3 u_world = axis_from_index(u_axis);
        const glm::vec3 v_world = axis_from_index(v_axis);

        const math::Vector2u vp_size = vp->GetViewport();
        if (vp_size.x > 0 && vp_size.y > 0)
        {
            const float ref_len   = std::max(0.1f, GIZMO_TWO_AXIS_OFFSET * kAssetVisualScale);
            const math::Vector3f p0 = drag.start_position;
            const math::Vector2i s0  = WorldPositionToScreen(p0, cam, vp_size);
            const math::Vector2i su  = WorldPositionToScreen(p0 + u_world * ref_len, cam, vp_size);
            const math::Vector2i sv  = WorldPositionToScreen(p0 + v_world * ref_len, cam, vp_size);

            const glm::vec2 du(static_cast<float>(su.x - s0.x), static_cast<float>(su.y - s0.y));
            const glm::vec2 dv(static_cast<float>(sv.x - s0.x), static_cast<float>(sv.y - s0.y));
            const glm::vec2 md(static_cast<float>(mouse.x - drag.start_mouse.x),
                               static_cast<float>(mouse.y - drag.start_mouse.y));

            const float det = du.x * dv.y - du.y * dv.x;
            if (std::fabs(det) > 1e-5f)
            {
                const float a = (md.x * dv.y - md.y * dv.x) / det;
                const float b = (du.x * md.y - du.y * md.x) / det;
                root_transform->SetLocalPosition(drag.start_position
                                                 + u_world * (a * ref_len)
                                                 + v_world * (b * ref_len));
            }
            else
            {
                // Degenerate screen projection — fall through to screen-plane drag.
                goto screen_plane_drag;
            }
            return;
        }
    }

screen_plane_drag:
    {
        // Centre handle or degenerate case: move in camera-aligned screen plane.
        math::Vector3f cam_right = math::AxisVector::X;
        math::Vector3f cam_up    = math::AxisVector::Y;
        if (cam)
        {
            cam_right = glm::normalize(math::Vector3f(cam->view[0][0], cam->view[1][0], cam->view[2][0]));
            cam_up    = glm::normalize(math::Vector3f(cam->view[0][1], cam->view[1][1], cam->view[2][1]));
        }

        const glm::vec3 drag_right = local_space
                                   ? glm::normalize(drag.start_rotation * math::AxisVector::X)
                                   : cam_right;
        const glm::vec3 drag_up    = local_space
                                   ? glm::normalize(drag.start_rotation * math::AxisVector::Y)
                                   : cam_up;

        const float dx = static_cast<float>(mouse.x - drag.start_mouse.x);
        const float dy = static_cast<float>(mouse.y - drag.start_mouse.y);
        root_transform->SetLocalPosition(drag.start_position
                                         + drag_right * (dx *  kMoveSensitivity)
                                         + drag_up    * (dy * -kMoveSensitivity));
    }
}

void MoveGizmoMode::EndDrag()
{
    if (drag.mouse_captured && drag.capture_input_sys)
        drag.capture_input_sys->EndMouseCapture(this);

    drag = GizmoDragState{};
    hovered_index = -1;
}

void MoveGizmoMode::RecoverIfOrphaned(bool left_down)
{
    if (drag.active && !left_down)
        EndDrag();
}
