// MoveGizmoMode — visual lifecycle methods.
// Included as part of GizmoUnified.cpp (single-TU pattern), so static helpers
// from GizmoUnified.AssetVisual.inl are in scope.

void MoveGizmoMode::BuildVisual(hgl::ecs::ECSContext *world,
                                 hgl::ecs::Entity *parent,
                                 std::vector<hgl::ecs::EntityID> &entity_ids)
{
    if (!world || !parent)
        return;

    MakeAndAttachPrimitive(primitives, world, parent, entity_ids,
                           "GizmoAssetMove_Center",
                           math::Vector3f(0.0f),
                           glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                           math::Vector3f(1.0f) * kAssetVisualScale,
                           GizmoShape::Sphere, GizmoColor::White);

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

        MakeAndAttachPrimitive(primitives, world, parent, entity_ids,
                               "GizmoAssetMove_Cylinder",
                               cfg.axis * GIZMO_CYLINDER_OFFSET * kAssetVisualScale,
                               rotation,
                               math::Vector3f(GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_HALF_LENGTH) * kAssetVisualScale,
                               GizmoShape::Cylinder, cfg.color, i);

        MakeAndAttachPrimitive(primitives, world, parent, entity_ids,
                               "GizmoAssetMove_Cone",
                               cfg.axis * GIZMO_CONE_OFFSET * kAssetVisualScale,
                               rotation,
                               math::Vector3f(1.0f) * kAssetVisualScale,
                               GizmoShape::Cone, cfg.color, i);

        MakeAndAttachPrimitive(primitives, world, parent, entity_ids,
                               "GizmoAssetMove_Plane",
                               cfg.plane_pos * kAssetVisualScale,
                               rotation,
                               math::Vector3f(2.0f) * kAssetVisualScale,
                               GizmoShape::Square, cfg.color);
    }
}

void MoveGizmoMode::DestroyVisual()
{
    primitives.clear();
    asset_instance.reset();
    entity = nullptr;
    hovered_index = -1;
    drag = MoveDragState{};
}

void MoveGizmoMode::SetVisible(bool visible)
{
    for (auto &entry : primitives)
    {
        if (entry.primitive)
            entry.primitive->SetVisible(visible);
    }
}
