static void BuildScaleAssetVisual(GizmoECS *gizmo, hgl::ecs::Entity *parent)
{
    if (!gizmo || !parent)
        return;

    if (auto *center = CreateAssetVisualEntity(gizmo,
                                               parent,
                                               "GizmoAssetScale_Center",
                                               math::Vector3f(0.0f),
                                               glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                               math::Vector3f(GIZMO_CENTER_SPHERE_RADIUS * 2.0f) * kAssetVisualScale))
    {
        AttachAssetModePrimitive(gizmo->scale_primitives, center, GizmoShape::Cube, GizmoColor::White);
    }

    struct AxisConfig
    {
        math::Vector3f axis;
        math::Vector3f rotation_axis;
        float rotation_deg;
        GizmoColor color;
        math::Vector3f plane_pos;
    };

    const AxisConfig configs[3] =
    {
        {math::AxisVector::X, math::Vector3f(0.0f, 1.0f, 0.0f),  90.0f, GizmoColor::Red,   math::Vector3f(0.0f, GIZMO_TWO_AXIS_OFFSET, GIZMO_TWO_AXIS_OFFSET)},
        {math::AxisVector::Y, math::Vector3f(1.0f, 0.0f, 0.0f), -90.0f, GizmoColor::Green, math::Vector3f(GIZMO_TWO_AXIS_OFFSET, 0.0f, GIZMO_TWO_AXIS_OFFSET)},
        {math::AxisVector::Z, math::Vector3f(0.0f, 0.0f, 1.0f),   0.0f, GizmoColor::Blue,  math::Vector3f(GIZMO_TWO_AXIS_OFFSET, GIZMO_TWO_AXIS_OFFSET, 0.0f)}
    };

    for (int i = 0; i < 3; ++i)
    {
        const auto &cfg = configs[i];
        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        if (cfg.rotation_deg != 0.0f)
            rotation = glm::angleAxis(glm::radians(cfg.rotation_deg), glm::vec3(cfg.rotation_axis));

        if (auto *cyl = CreateAssetVisualEntity(gizmo,
                                                parent,
                                                "GizmoAssetScale_Cylinder",
                                                cfg.axis * GIZMO_CYLINDER_OFFSET * kAssetVisualScale,
                                                rotation,
                                                math::Vector3f(GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_HALF_LENGTH) * kAssetVisualScale))
        {
            AttachAssetModePrimitive(gizmo->scale_primitives, cyl, GizmoShape::Cylinder, cfg.color, i);
        }

        if (auto *tip = CreateAssetVisualEntity(gizmo,
                                                parent,
                                                "GizmoAssetScale_CubeTip",
                                                cfg.axis * GIZMO_CONE_OFFSET * kAssetVisualScale,
                                                rotation,
                                                math::Vector3f(1.0f) * kAssetVisualScale))
        {
            AttachAssetModePrimitive(gizmo->scale_primitives, tip, GizmoShape::Cube, cfg.color, i);
        }

        if (auto *plane = CreateAssetVisualEntity(gizmo,
                                                  parent,
                                                  "GizmoAssetScale_Plane",
                                                  cfg.plane_pos * kAssetVisualScale,
                                                  rotation,
                                                  math::Vector3f(2.0f) * kAssetVisualScale))
        {
            AttachAssetModePrimitive(gizmo->scale_primitives, plane, GizmoShape::Square, cfg.color);
        }
    }
}