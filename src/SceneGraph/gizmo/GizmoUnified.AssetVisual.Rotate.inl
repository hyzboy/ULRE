static void BuildRotateAssetVisual(GizmoECS *gizmo, hgl::ecs::Entity *parent)
{
    if (!gizmo || !parent)
        return;

    struct AxisConfig
    {
        math::Vector3f rotation_axis;
        float rotation_deg;
        GizmoColor color;
    };

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

        if (auto *torus = CreateAssetVisualEntity(gizmo,
                                                  parent,
                                                  "GizmoAssetRotate_Torus",
                                                  math::Vector3f(0.0f),
                                                  rotation,
                                                  math::Vector3f(GIZMO_ARROW_LENGTH) * kAssetVisualScale))
        {
            AttachAssetModePrimitive(gizmo->rotate_primitives, torus, GizmoShape::Torus, cfg.color, i);
        }
    }

    if (auto *white_torus = CreateAssetVisualEntity(gizmo,
                                                    parent,
                                                    "GizmoAssetRotate_WhiteTorus",
                                                    math::Vector3f(0.0f),
                                                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                                    math::Vector3f(13.0f) * kAssetVisualScale,
                                                    &gizmo->rotate_white_ring_transform))
    {
        AttachAssetModePrimitive(gizmo->rotate_primitives, white_torus, GizmoShape::Torus, GizmoColor::White, 3);
    }
}