// ScaleGizmoMode — visual lifecycle methods.
// Included as part of GizmoUnified.cpp (single-TU pattern), so static helpers
// from GizmoUnified.AssetVisual.inl are in scope.

void ScaleGizmoMode::BuildVisual(hgl::ecs::ECSContext *world,
                                  hgl::ecs::Entity *parent,
                                  std::vector<hgl::ecs::EntityID> &entity_ids)
{
    if (!world || !parent)
        return;

    // Center cube (white, group -1 = uniform scale on no-pick path).
    if (auto *center = CreateAssetVisualEntity(world, entity_ids,
                                               parent,
                                               "GizmoAssetScale_Center",
                                               math::Vector3f(0.0f),
                                               glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                               math::Vector3f(GIZMO_CENTER_SPHERE_RADIUS * 2.0f) * kAssetVisualScale))
    {
        AttachAssetModePrimitive(primitives, center, GizmoShape::Cube, GizmoColor::White);
    }

    struct AxisConfig
    {
        math::Vector3f axis;
        math::Vector3f rotation_axis;
        float          rotation_deg;
        GizmoColor     color;
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

        if (auto *cyl = CreateAssetVisualEntity(world, entity_ids,
                                                parent,
                                                "GizmoAssetScale_Cylinder",
                                                cfg.axis * GIZMO_CYLINDER_OFFSET * kAssetVisualScale,
                                                rotation,
                                                math::Vector3f(GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_HALF_LENGTH) * kAssetVisualScale))
        {
            AttachAssetModePrimitive(primitives, cyl, GizmoShape::Cylinder, cfg.color, i);
        }

        if (auto *tip = CreateAssetVisualEntity(world, entity_ids,
                                                parent,
                                                "GizmoAssetScale_CubeTip",
                                                cfg.axis * GIZMO_CONE_OFFSET * kAssetVisualScale,
                                                rotation,
                                                math::Vector3f(1.0f) * kAssetVisualScale))
        {
            AttachAssetModePrimitive(primitives, tip, GizmoShape::Cube, cfg.color, i);
        }

        if (auto *plane = CreateAssetVisualEntity(world, entity_ids,
                                                  parent,
                                                  "GizmoAssetScale_Plane",
                                                  cfg.plane_pos * kAssetVisualScale,
                                                  rotation,
                                                  math::Vector3f(2.0f) * kAssetVisualScale))
        {
            AttachAssetModePrimitive(primitives, plane, GizmoShape::Square, cfg.color);
        }
    }
}

void ScaleGizmoMode::DestroyVisual()
{
    primitives.clear();
    asset_instance.reset();
    entity = nullptr;
    hovered_index = -1;
    drag = ScaleDragState{};
}

void ScaleGizmoMode::SetVisible(bool visible)
{
    for (auto &entry : primitives)
    {
        if (entry.primitive)
            entry.primitive->SetVisible(visible);
    }
}
