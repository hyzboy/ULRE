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
    PrimitiveDesc center;
    center.name  = "GizmoAssetScale_Center";
    center.pos   = math::Vector3f(0.0f);
    center.scale = math::Vector3f(GIZMO_CENTER_SPHERE_RADIUS * 2.0f) * kAssetVisualScale;
    center.shape = GizmoShape::Cube;
    center.color = GizmoColor::White;
    MakeAndAttachPrimitive(primitives, world, parent, entity_ids, center);

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

        PrimitiveDesc cylinder;
        cylinder.name     = "GizmoAssetScale_Cylinder";
        cylinder.pos      = cfg.axis * GIZMO_CYLINDER_OFFSET * kAssetVisualScale;
        cylinder.rot      = rotation;
        cylinder.scale    = math::Vector3f(GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_HALF_LENGTH) * kAssetVisualScale;
        cylinder.shape    = GizmoShape::Cylinder;
        cylinder.color    = cfg.color;
        cylinder.group_id = i;
        MakeAndAttachPrimitive(primitives, world, parent, entity_ids, cylinder);

        PrimitiveDesc cube_tip;
        cube_tip.name     = "GizmoAssetScale_CubeTip";
        cube_tip.pos      = cfg.axis * GIZMO_CONE_OFFSET * kAssetVisualScale;
        cube_tip.rot      = rotation;
        cube_tip.scale    = math::Vector3f(1.0f) * kAssetVisualScale;
        cube_tip.shape    = GizmoShape::Cube;
        cube_tip.color    = cfg.color;
        cube_tip.group_id = i;
        MakeAndAttachPrimitive(primitives, world, parent, entity_ids, cube_tip);

        PrimitiveDesc plane;
        plane.name  = "GizmoAssetScale_Plane";
        plane.pos   = cfg.plane_pos * kAssetVisualScale;
        plane.rot   = rotation;
        plane.scale = math::Vector3f(2.0f) * kAssetVisualScale;
        plane.shape = GizmoShape::Square;
        plane.color = cfg.color;
        MakeAndAttachPrimitive(primitives, world, parent, entity_ids, plane);
    }
}

void ScaleGizmoMode::DestroyVisual()
{
    primitives.clear();
    entity = nullptr;
    hovered_index = -1;
    drag = GizmoDragState{};
}

void ScaleGizmoMode::SetVisible(bool visible)
{
    SetPrimitivesVisible(primitives, visible);
}
