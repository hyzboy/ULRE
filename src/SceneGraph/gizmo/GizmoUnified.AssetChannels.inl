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

#include "GizmoUnified.AssetChannels.Move.inl"
#include "GizmoUnified.AssetChannels.Rotate.inl"
#include "GizmoUnified.AssetChannels.Scale.inl"
