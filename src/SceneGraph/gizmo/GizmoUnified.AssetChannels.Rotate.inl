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
            const bool local_space = GizmoController::IsRotateMode(gizmo->asset_drag.mode) && GizmoController::IsLocalMode(gizmo->asset_drag.mode);
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
    if (GizmoController::IsRotateMode(gizmo->asset_drag.mode) && GizmoController::IsWorldMode(gizmo->asset_drag.mode))
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