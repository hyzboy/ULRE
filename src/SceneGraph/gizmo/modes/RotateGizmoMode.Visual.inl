// RotateGizmoMode — visual lifecycle methods.
// Included as part of GizmoUnified.cpp (single-TU pattern), so static helpers
// from GizmoUnified.AssetVisual.inl are in scope.

void RotateGizmoMode::BuildVisual(hgl::ecs::ECSContext *world,
                                   hgl::ecs::Entity *parent,
                                   std::vector<hgl::ecs::EntityID> &entity_ids)
{
    if (!world || !parent)
        return;

    struct AxisConfig
    {
        math::Vector3f rotation_axis;
        float          rotation_deg;
        GizmoColor     color;
    };

    // X-axis ring (Red): lies in YZ plane — no rotation needed (Torus default = XZ plane, so
    // rotate 0° around Z keeps it in the XZ plane; group 0 = X axis => ring normal is X).
    // Y-axis ring (Green): rotate 90° around Z so ring normal is Y.
    // Z-axis ring (Blue): rotate 90° around Y so ring normal is Z.
    const AxisConfig configs[3] =
    {
        {math::Vector3f(0.0f, 0.0f, 1.0f),   0.0f, GizmoColor::Red},
        {math::Vector3f(0.0f, 0.0f, 1.0f),  90.0f, GizmoColor::Green},
        {math::Vector3f(0.0f, 1.0f, 0.0f),  90.0f, GizmoColor::Blue}
    };

    for (int i = 0; i < 3; ++i)
    {
        const auto &cfg = configs[i];
        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        if (cfg.rotation_deg != 0.0f)
            rotation = glm::angleAxis(glm::radians(cfg.rotation_deg), glm::vec3(cfg.rotation_axis));

        PrimitiveDesc torus;
        torus.name     = "GizmoAssetRotate_Torus";
        torus.pos      = math::Vector3f(0.0f);
        torus.rot      = rotation;
        torus.scale    = math::Vector3f(GIZMO_ARROW_LENGTH) * kAssetVisualScale;
        torus.shape    = GizmoShape::Torus;
        torus.color    = cfg.color;
        torus.group_id = i;
        MakeAndAttachPrimitive(primitives, world, parent, entity_ids, torus);
    }

    // White view-facing ring (group_id = 3, larger scale, aux_transform captured).
    PrimitiveDesc white_torus;
    white_torus.name          = "GizmoAssetRotate_WhiteTorus";
    white_torus.pos           = math::Vector3f(0.0f);
    white_torus.scale         = math::Vector3f(13.0f) * kAssetVisualScale;
    white_torus.shape         = GizmoShape::Torus;
    white_torus.color         = GizmoColor::White;
    white_torus.group_id      = 3;
    white_torus.out_transform = &aux_transform;
    MakeAndAttachPrimitive(primitives, world, parent, entity_ids, white_torus);
}

void RotateGizmoMode::DestroyVisual()
{
    primitives.clear();
    aux_transform.reset();
    asset_instance.reset();
    entity = nullptr;
    hovered_index = -1;
    drag = GizmoDragState{};
}

void RotateGizmoMode::SetVisible(bool visible)
{
    SetPrimitivesVisible(primitives, visible);
}
