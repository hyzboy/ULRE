static glm::vec3 AssetAxisFromIndex(const GizmoECS *gizmo, int axis_index, bool local_space)
{
    glm::vec3 axis = math::GetAxisVector(math::AXIS(axis_index));
    if (local_space)
        axis = gizmo->asset_drag.start_rotation * axis;
    return glm::normalize(axis);
}

static void AssetPlaneAxesFromNormal(int normal_axis, int &u_axis, int &v_axis)
{
    switch (normal_axis)
    {
    case 0: u_axis = 1; v_axis = 2; break; // YZ plane
    case 1: u_axis = 0; v_axis = 2; break; // XZ plane
    case 2: u_axis = 0; v_axis = 1; break; // XY plane
    default: u_axis = 0; v_axis = 1; break;
    }
}

static float AssetProjectMouseDeltaToAxisPixels(const GizmoECS *gizmo,
                                                const math::Vector2i &mouse_coord,
                                                const CameraInfo *camera_info,
                                                const ViewportInfo *viewport_info,
                                                const glm::vec3 &axis_world)
{
    if (!camera_info || !viewport_info)
        return 0.0f;

    const math::Vector2u viewport_size = viewport_info->GetViewport();
    if (viewport_size.x == 0 || viewport_size.y == 0)
        return 0.0f;

    const math::Vector3f p0 = gizmo->asset_drag.start_position;
    const math::Vector3f p1 = p0 + axis_world * (GIZMO_ARROW_LENGTH * kAssetVisualScale);
    const math::Vector2i s0 = WorldPositionToScreen(p0, camera_info, viewport_size);
    const math::Vector2i s1 = WorldPositionToScreen(p1, camera_info, viewport_size);

    glm::vec2 dir(static_cast<float>(s1.x - s0.x), static_cast<float>(s1.y - s0.y));
    const float len = glm::length(dir);
    if (len < 1e-4f)
        return 0.0f;

    dir /= len;
    const glm::vec2 mouse_delta(static_cast<float>(mouse_coord.x - gizmo->asset_drag.start_mouse.x),
                                static_cast<float>(mouse_coord.y - gizmo->asset_drag.start_mouse.y));
    return glm::dot(mouse_delta, dir);
}

static float ComputeAssetRotationDelta(const GizmoECS *gizmo,
                                       const math::Vector2i &mouse_coord,
                                       const CameraInfo *camera_info,
                                       const ViewportInfo *viewport_info,
                                       float dx,
                                       float dy,
                                       float rotate_sensitivity)
{
    if (!camera_info || !viewport_info)
        return (-dx - dy) * rotate_sensitivity;

    const math::Vector2u viewport_size = viewport_info->GetViewport();
    if (viewport_size.x == 0 || viewport_size.y == 0)
        return (-dx - dy) * rotate_sensitivity;

    const math::Vector2i center = WorldPositionToScreen(gizmo->asset_drag.start_position, camera_info, viewport_size);
    const glm::vec2 c(static_cast<float>(center.x), static_cast<float>(center.y));
    const glm::vec2 v0(static_cast<float>(gizmo->asset_drag.start_mouse.x) - c.x,
                       static_cast<float>(gizmo->asset_drag.start_mouse.y) - c.y);
    const glm::vec2 v1(static_cast<float>(mouse_coord.x) - c.x,
                       static_cast<float>(mouse_coord.y) - c.y);

    if (glm::length(v0) < 4.0f || glm::length(v1) < 4.0f)
        return (-dx - dy) * rotate_sensitivity;

    const float cross_z = v0.x * v1.y - v0.y * v1.x;
    const float dot_v = glm::dot(v0, v1);
    return std::atan2(cross_z, dot_v);
}

static void ApplyAssetMoveDragChannel(GizmoECS *gizmo,
                                      const math::Vector2i &mouse_coord,
                                      const CameraInfo *camera_info,
                                      const ViewportInfo *viewport_info,
                                      const glm::vec3 &camera_right,
                                      const glm::vec3 &camera_up,
                                      float dx,
                                      float dy,
                                      float move_sensitivity)
{
    if (gizmo->asset_drag.pick_group >= 0 && gizmo->asset_drag.pick_group < 3)
    {
        const bool local_space = (gizmo->asset_drag.mode == GizmoMode::MoveLocal);
        const glm::vec3 move_axis = AssetAxisFromIndex(gizmo, gizmo->asset_drag.pick_group, local_space);
        float delta_world = AssetProjectMouseDeltaToAxisPixels(gizmo, mouse_coord, camera_info, viewport_info, move_axis);

        if (camera_info && viewport_info)
        {
            const float wupp = gizmo->root_transform->ComputeWorldUnitsPerPixel(camera_info, viewport_info);
            if (wupp > 0.0f)
                delta_world *= wupp;
            else
                delta_world *= move_sensitivity;
        }
        else
        {
            delta_world *= move_sensitivity;
        }

        gizmo->root_transform->SetLocalPosition(gizmo->asset_drag.start_position + move_axis * delta_world);
        return;
    }

    if (gizmo->asset_drag.pick_shape == GizmoShape::Square &&
        gizmo->asset_drag.pick_plane_normal_axis >= 0 &&
        camera_info && viewport_info)
    {
        const bool local_space = (gizmo->asset_drag.mode == GizmoMode::MoveLocal);
        int u_axis = 0;
        int v_axis = 1;
        AssetPlaneAxesFromNormal(gizmo->asset_drag.pick_plane_normal_axis, u_axis, v_axis);

        const glm::vec3 u_world = AssetAxisFromIndex(gizmo, u_axis, local_space);
        const glm::vec3 v_world = AssetAxisFromIndex(gizmo, v_axis, local_space);

        const math::Vector2u viewport_size = viewport_info->GetViewport();
        if (viewport_size.x > 0 && viewport_size.y > 0)
        {
            const float ref_len = std::max(0.1f, GIZMO_TWO_AXIS_OFFSET * kAssetVisualScale);
            const math::Vector3f p0 = gizmo->asset_drag.start_position;
            const math::Vector3f pu = p0 + u_world * ref_len;
            const math::Vector3f pv = p0 + v_world * ref_len;

            const math::Vector2i s0 = WorldPositionToScreen(p0, camera_info, viewport_size);
            const math::Vector2i su = WorldPositionToScreen(pu, camera_info, viewport_size);
            const math::Vector2i sv = WorldPositionToScreen(pv, camera_info, viewport_size);

            const glm::vec2 du(static_cast<float>(su.x - s0.x), static_cast<float>(su.y - s0.y));
            const glm::vec2 dv(static_cast<float>(sv.x - s0.x), static_cast<float>(sv.y - s0.y));
            const glm::vec2 md(static_cast<float>(mouse_coord.x - gizmo->asset_drag.start_mouse.x),
                               static_cast<float>(mouse_coord.y - gizmo->asset_drag.start_mouse.y));

            const float det = du.x * dv.y - du.y * dv.x;
            if (std::fabs(det) > 1e-5f)
            {
                const float a = (md.x * dv.y - md.y * dv.x) / det;
                const float b = (du.x * md.y - du.y * md.x) / det;
                const glm::vec3 world_delta = u_world * (a * ref_len) + v_world * (b * ref_len);
                gizmo->root_transform->SetLocalPosition(gizmo->asset_drag.start_position + world_delta);
            }
            else
            {
                const glm::vec3 world_delta = camera_right * (dx * move_sensitivity)
                                            + camera_up * (-dy * move_sensitivity);
                gizmo->root_transform->SetLocalPosition(gizmo->asset_drag.start_position + world_delta);
            }
        }

        return;
    }

    // Center/plane fallback: move in camera-aligned screen plane.
    const glm::vec3 drag_right = (gizmo->asset_drag.mode == GizmoMode::MoveLocal)
                               ? glm::normalize(gizmo->asset_drag.start_rotation * math::AxisVector::X)
                               : camera_right;
    const glm::vec3 drag_up = (gizmo->asset_drag.mode == GizmoMode::MoveLocal)
                            ? glm::normalize(gizmo->asset_drag.start_rotation * math::AxisVector::Y)
                            : camera_up;

    const glm::vec3 world_delta = drag_right * (dx * move_sensitivity)
                                + drag_up * (-dy * move_sensitivity);
    gizmo->root_transform->SetLocalPosition(gizmo->asset_drag.start_position + world_delta);
}

static void ApplyAssetRotateDragChannel(GizmoECS *gizmo,
                                        const math::Vector2i &mouse_coord,
                                        const CameraInfo *camera_info,
                                        const ViewportInfo *viewport_info,
                                        const glm::vec3 &camera_right,
                                        const glm::vec3 &camera_up,
                                        float dx,
                                        float dy,
                                        float rotate_sensitivity)
{
    if (gizmo->asset_drag.pick_group >= 0)
    {
        const float delta_angle = ComputeAssetRotationDelta(gizmo, mouse_coord, camera_info, viewport_info, dx, dy, rotate_sensitivity);
        glm::vec3 axis = math::AxisVector::Z;

        if (gizmo->asset_drag.pick_group < 3)
        {
            const bool local_space = (gizmo->asset_drag.mode == GizmoMode::RotateLocal);
            axis = AssetAxisFromIndex(gizmo, gizmo->asset_drag.pick_group, local_space);
        }
        else if (camera_info)
        {
            axis = glm::normalize(math::Vector3f(camera_info->view[0][2],
                                                 camera_info->view[1][2],
                                                 camera_info->view[2][2]));
        }

        const glm::quat dq = glm::angleAxis(delta_angle, glm::normalize(axis));
        gizmo->root_transform->SetLocalRotation(glm::normalize(dq * gizmo->asset_drag.start_rotation));
        return;
    }

    // Headless/no-pick fallback to previous deterministic drag behavior.
    if (gizmo->asset_drag.mode == GizmoMode::RotateWorld)
    {
        const glm::quat yaw = glm::angleAxis(-dx * rotate_sensitivity, math::AxisVector::Y);
        const glm::quat pitch = glm::angleAxis(-dy * rotate_sensitivity, camera_right);
        gizmo->root_transform->SetLocalRotation(glm::normalize(yaw * pitch * gizmo->asset_drag.start_rotation));
    }
    else
    {
        const glm::vec3 local_x = gizmo->asset_drag.start_rotation * camera_right;
        const glm::vec3 local_y = gizmo->asset_drag.start_rotation * camera_up;
        const glm::quat yaw_local = glm::angleAxis(-dx * rotate_sensitivity, local_y);
        const glm::quat pitch_local = glm::angleAxis(-dy * rotate_sensitivity, local_x);
        gizmo->root_transform->SetLocalRotation(glm::normalize(yaw_local * pitch_local * gizmo->asset_drag.start_rotation));
    }
}

static void ApplyAssetScaleDragChannel(GizmoECS *gizmo,
                                       const math::Vector2i &mouse_coord,
                                       const CameraInfo *camera_info,
                                       const ViewportInfo *viewport_info,
                                       float dy,
                                       float scale_sensitivity,
                                       const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                                       bool has_view_context,
                                       math::Vector3f &cur_effective_scale)
{
    glm::vec3 s = gizmo->asset_drag.start_scale;

    if (gizmo->asset_drag.pick_group >= 0 && gizmo->asset_drag.pick_group < 3)
    {
        const glm::vec3 scale_axis = AssetAxisFromIndex(gizmo, gizmo->asset_drag.pick_group, true);
        const float axis_pixels = AssetProjectMouseDeltaToAxisPixels(gizmo, mouse_coord, camera_info, viewport_info, scale_axis);
        const float ratio = std::clamp(1.0f + axis_pixels * scale_sensitivity, 0.05f, 10.0f);
        s[gizmo->asset_drag.pick_group] *= ratio;
    }
    else if (gizmo->asset_drag.pick_shape == GizmoShape::Square && gizmo->asset_drag.pick_plane_normal_axis >= 0)
    {
        const float ratio = std::clamp(1.0f + (-dy) * scale_sensitivity, 0.05f, 10.0f);
        if (gizmo->asset_drag.pick_plane_normal_axis == 0)
        {
            s.y *= ratio;
            s.z *= ratio;
        }
        else if (gizmo->asset_drag.pick_plane_normal_axis == 1)
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
    else
    {
        const float ratio = std::clamp(1.0f + (-dy) * scale_sensitivity, 0.05f, 10.0f);
        s *= ratio;
    }

    NormalizeScaleByPolicy(s, gizmo->allow_negative_scale);
    if (target_transform)
    {
        cur_effective_scale = s;
        if (!has_view_context)
            gizmo->root_transform->SetLocalScale(s);
    }
    else
    {
        gizmo->root_transform->SetLocalScale(s);
    }
}
