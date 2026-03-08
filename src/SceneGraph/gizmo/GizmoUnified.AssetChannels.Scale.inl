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