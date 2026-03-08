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