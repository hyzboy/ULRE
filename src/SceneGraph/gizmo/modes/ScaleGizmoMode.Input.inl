// ScaleGizmoMode — interactive (hover + drag) methods.
// Included as part of GizmoUnified.cpp (single-TU pattern), so static helpers
// from GizmoUnified.AssetVisual.inl and GizmoUnified.AssetChannels.inl are in scope.

void ScaleGizmoMode::UpdateHover(const GizmoFrameInput &input,
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
            p.primitive->GetPrimitive()->ChangeMaterialInstance(GetGizmoMI3D(GizmoColor::Yellow));
        else
            p.primitive->GetPrimitive()->ChangeMaterialInstance(p.base_material);
    }
}

bool ScaleGizmoMode::TryBeginDrag(const GizmoFrameInput &input,
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

void ScaleGizmoMode::ApplyDrag(const GizmoFrameInput &input,
                                bool allow_negative_scale,
                                const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                                const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform,
                                math::Vector3f &cur_effective_scale)
{
    const math::Vector2i &mouse = input.mouse_coord;
    const CameraInfo    *cam   = input.camera_info;
    const ViewportInfo  *vp    = input.viewport_info;
    const bool has_view_context = (cam && vp);
    if (!root_transform || !drag.active)
        return;

    constexpr float kScaleSensitivity = 0.01f;
    const float dy = static_cast<float>(mouse.y - drag.start_mouse.y);

    glm::vec3 s = drag.start_scale;

    // ─── Axis drag (Cylinder / Cube tip — group 0/1/2) ───────────────────
    if (drag.pick.pick_group >= 0 && drag.pick.pick_group < 3)
    {
        // Scale is always local — axis follows object orientation.
        glm::vec3 axis = math::GetAxisVector(math::AXIS(drag.pick.pick_group));
        axis = glm::normalize(drag.start_rotation * axis);

        // Project mouse delta onto the screen-space projection of the axis.
        float axis_pixels = 0.0f;
        if (cam && vp)
        {
            const math::Vector2u vp_size = vp->GetViewport();
            if (vp_size.x > 0 && vp_size.y > 0)
            {
                const math::Vector3f p0 = drag.start_position;
                const math::Vector3f p1 = p0 + axis * (GIZMO_ARROW_LENGTH * kAssetVisualScale);
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
                    axis_pixels = glm::dot(md, dir);
                }
            }
        }

        const float ratio = std::clamp(1.0f + axis_pixels * kScaleSensitivity, 0.05f, 10.0f);
        s[drag.pick.pick_group] *= ratio;
    }
    // ─── Plane drag (Square pick) ─────────────────────────────────────────
    else if (drag.pick.pick_shape == GizmoShape::Square
             && drag.pick.pick_plane_normal_axis >= 0)
    {
        const float ratio = std::clamp(1.0f + (-dy) * kScaleSensitivity, 0.05f, 10.0f);
        if (drag.pick.pick_plane_normal_axis == 0)
        {
            s.y *= ratio;
            s.z *= ratio;
        }
        else if (drag.pick.pick_plane_normal_axis == 1)
        {
            s.x *= ratio;
            s.z *= ratio;
        }
        else
        {
            s.x *= ratio;
            s.y *= ratio;
        }
    }
    // ─── Uniform scale (center cube / no pick) ───────────────────────────
    else
    {
        const float ratio = std::clamp(1.0f + (-dy) * kScaleSensitivity, 0.05f, 10.0f);
        s *= ratio;
    }

    NormalizeScaleByPolicy(s, allow_negative_scale);

    if (target_transform)
    {
        cur_effective_scale = s;
        if (!has_view_context)
            root_transform->SetLocalScale(s);
    }
    else
    {
        root_transform->SetLocalScale(s);
    }
}

void ScaleGizmoMode::EndDrag()
{
    if (drag.mouse_captured && drag.capture_input_sys)
        drag.capture_input_sys->EndMouseCapture(this);

    drag = GizmoDragState{};
    hovered_index = -1;
}

void ScaleGizmoMode::RecoverIfOrphaned(bool left_down)
{
    if (drag.active && !left_down)
        EndDrag();
}
