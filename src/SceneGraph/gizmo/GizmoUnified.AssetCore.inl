static float SanitizeFixedPixelDiameter(float pixel_diameter)
{
    if (pixel_diameter < 16.0f)
        return 16.0f;

    if (pixel_diameter > 4096.0f)
        return 4096.0f;

    return pixel_diameter;
}

static void ApplyAssetFixedPixelSizingParameters(GizmoECS *gizmo)
{
    if (!gizmo)
        return;

    constexpr float kReferenceWorldDiameter = GIZMO_ARROW_LENGTH * 2.0f;
    constexpr float kMinScale = 0.01f;

    const auto apply_to_entity = [gizmo](hgl::ecs::Entity *entity,
                                         const float reference_world_diameter,
                                         const float min_scale)
    {
        if (!entity)
            return;

        auto t = entity->GetComponent<hgl::ecs::TransformComponent>();
        if (!t)
            return;

        t->SetFixedPixelSizingParameters(gizmo->fixed_pixel_diameter,
                                         reference_world_diameter,
                                         min_scale);
        t->SetFixedPixelSizingEnabled(true);
    };

    apply_to_entity(gizmo->move_mode.entity, kReferenceWorldDiameter, kMinScale);
    apply_to_entity(gizmo->rotate_mode.entity, kReferenceWorldDiameter, kMinScale);
    apply_to_entity(gizmo->scale_mode.entity, kReferenceWorldDiameter, kMinScale);
}

static void SyncGizmoAssetModeBindings(GizmoECS *gizmo)
{
    if (!gizmo)
        return;

    const bool move_active = gizmo->root_visible && IsMoveMode(gizmo->current_mode);
    const bool rotate_active = gizmo->root_visible && IsRotateMode(gizmo->current_mode);
    const bool scale_active = gizmo->root_visible && IsScaleMode(gizmo->current_mode);

    for (auto &entry : gizmo->move_mode.primitives)
    {
        if (entry.primitive)
            entry.primitive->SetVisible(move_active);
    }

    for (auto &entry : gizmo->rotate_mode.primitives)
    {
        if (entry.primitive)
            entry.primitive->SetVisible(rotate_active);
    }

    for (auto &entry : gizmo->scale_mode.primitives)
    {
        if (entry.primitive)
            entry.primitive->SetVisible(scale_active);
    }
}

static void SyncAssetSubGizmoLocalTransforms(GizmoECS *gizmo)
{
    if (!gizmo || !gizmo->root_transform)
        return;

    const glm::quat root_rot = gizmo->root_transform->GetLocalRotation();
    const glm::quat inv_root_rot = glm::inverse(root_rot);
    const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);

    auto set_child_rotation = [&](hgl::ecs::Entity *entity, const glm::quat &q)
    {
        if (!entity)
            return;

        auto t = entity->GetComponent<hgl::ecs::TransformComponent>();
        if (t)
            t->SetLocalRotation(q);
    };

    const bool move_local = IsMoveMode(gizmo->current_mode) && IsLocalMode(gizmo->current_mode);
    const bool rotate_local = IsRotateMode(gizmo->current_mode) && IsLocalMode(gizmo->current_mode);

    set_child_rotation(gizmo->move_mode.entity, move_local ? identity : inv_root_rot);
    set_child_rotation(gizmo->rotate_mode.entity, rotate_local ? identity : inv_root_rot);
    set_child_rotation(gizmo->scale_mode.entity, identity);
}

static void SyncAssetFixedPixelSizingContext(GizmoECS *gizmo,
                                             const CameraInfo *camera_info,
                                             const ViewportInfo *viewport_info)
{
    if (!gizmo)
        return;

    if (!camera_info || !viewport_info)
        return;

    auto apply_ctx = [&](hgl::ecs::Entity *entity)
    {
        if (!entity)
            return;

        auto t = entity->GetComponent<hgl::ecs::TransformComponent>();
        if (t && t->IsFixedPixelSizingEnabled())
            t->SetFixedPixelSizingContext(camera_info, viewport_info);
    };

    apply_ctx(gizmo->move_mode.entity);
    apply_ctx(gizmo->rotate_mode.entity);
    apply_ctx(gizmo->scale_mode.entity);
}

static int GetScalePlaneNormalAxisFromEntry(const GizmoVisualPrimitive &entry)
{
    if (!entry.transform || entry.shape != GizmoShape::Square)
        return -1;

    const math::Vector3f lp = entry.transform->GetLocalPosition();
    const float ax = std::fabs(lp.x);
    const float ay = std::fabs(lp.y);
    const float az = std::fabs(lp.z);

    if (ax <= ay && ax <= az) return 0; // YZ plane
    if (ay <= ax && ay <= az) return 1; // XZ plane
    return 2;                            // XY plane
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

