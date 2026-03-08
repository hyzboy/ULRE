static GizmoECS::AssetDragState::ChannelState &GetAssetChannelState(GizmoECS *gizmo, GizmoMode mode)
{
    switch (GizmoController::SlotForMode(mode))
    {
    case GizmoController::ChannelSlot::Move:
        return gizmo->asset_drag.move;
    case GizmoController::ChannelSlot::Rotate:
        return gizmo->asset_drag.rotate;
    case GizmoController::ChannelSlot::Scale:
    default:
        return gizmo->asset_drag.scale;
    }
}

static void ResetAssetActivePickState(GizmoECS *gizmo)
{
    gizmo->asset_drag.pick_index = -1;
    gizmo->asset_drag.pick_group = -1;
    gizmo->asset_drag.pick_plane_normal_axis = -1;
    gizmo->asset_drag.pick_shape = GizmoShape::Sphere;
}

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

    apply_to_entity(gizmo->MoveChannel().entity, kReferenceWorldDiameter, kMinScale);
    apply_to_entity(gizmo->RotateChannel().entity, kReferenceWorldDiameter, kMinScale);
    apply_to_entity(gizmo->ScaleChannel().entity, kReferenceWorldDiameter, kMinScale);
}

static void SyncGizmoAssetModeBindings(GizmoECS *gizmo)
{
    if (!gizmo)
        return;

    const bool move_active = gizmo->root_visible && GizmoController::IsMoveMode(gizmo->current_mode);
    const bool rotate_active = gizmo->root_visible && GizmoController::IsRotateMode(gizmo->current_mode);
    const bool scale_active = gizmo->root_visible && GizmoController::IsScaleMode(gizmo->current_mode);

    const uint32_t mode_code = static_cast<uint32_t>(gizmo->current_mode);

    auto apply_active = [gizmo, mode_code](const std::shared_ptr<hgl::ecs::AssetInstanceComponent> &comp,
                                           bool active,
                                           uint64_t base_payload)
    {
        if (!comp)
            return;

        // Keep pass id in low 8 bits; use high bit as an active marker for future backend policies.
        comp->SetFlags(active ? (1u | (1u << 31)) : 1u);
        comp->SetVisibilityMask(active ? ~0ull : 0ull);

        // Encode mode/active as payload metadata and bump revision so bridge can observe mode transitions.
        hgl::ecs::AssetOverrideRef ref = comp->GetOverrideRef();
        ref.payload_ref = base_payload ^ (static_cast<uint64_t>(mode_code) << 8) ^ (active ? 1ull : 0ull);
        ref.revision = ++gizmo->asset_mode_revision_counter;
        comp->SetOverrideRef(ref);
    };

    apply_active(gizmo->MoveChannel().asset_instance, move_active, kGizmoMoveOverrideRef);
    apply_active(gizmo->RotateChannel().asset_instance, rotate_active, kGizmoRotateOverrideRef);
    apply_active(gizmo->ScaleChannel().asset_instance, scale_active, kGizmoScaleOverrideRef);

    for (auto &entry : gizmo->MoveChannel().primitives)
    {
        if (entry.primitive)
            entry.primitive->SetVisible(move_active);
    }

    for (auto &entry : gizmo->RotateChannel().primitives)
    {
        if (entry.primitive)
            entry.primitive->SetVisible(rotate_active);
    }

    for (auto &entry : gizmo->ScaleChannel().primitives)
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

    const bool move_local = GizmoController::IsMoveMode(gizmo->current_mode) && GizmoController::IsLocalMode(gizmo->current_mode);
    const bool rotate_local = GizmoController::IsRotateMode(gizmo->current_mode) && GizmoController::IsLocalMode(gizmo->current_mode);

    // Child world rotation = root_rot * child_local_rot.
    // World mode wants axis fixed in world space -> child_local_rot = inverse(root_rot).
    // Local mode wants axis follow object/root space -> child_local_rot = identity.
    set_child_rotation(gizmo->MoveChannel().entity, move_local ? identity : inv_root_rot);
    set_child_rotation(gizmo->RotateChannel().entity, rotate_local ? identity : inv_root_rot);
    set_child_rotation(gizmo->ScaleChannel().entity, identity);
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

    apply_ctx(gizmo->MoveChannel().entity);
    apply_ctx(gizmo->RotateChannel().entity);
    apply_ctx(gizmo->ScaleChannel().entity);
}

static bool BeginAssetMouseCapture(GizmoECS *gizmo, hgl::ecs::InputSystem *input_system)
{
    if (!gizmo)
        return false;

    if (!input_system)
        return true;

    if (gizmo->asset_drag.mouse_captured)
    {
        if (gizmo->asset_drag.capture_input_system == input_system)
            return true;

        if (gizmo->asset_drag.capture_input_system)
            gizmo->asset_drag.capture_input_system->EndMouseCapture(gizmo);

        gizmo->asset_drag.mouse_captured = false;
        gizmo->asset_drag.capture_input_system = nullptr;
    }

    if (!input_system->BeginMouseCapture(gizmo))
        return false;

    gizmo->asset_drag.mouse_captured = true;
    gizmo->asset_drag.capture_input_system = input_system;
    return true;
}

static void EndAssetMouseCapture(GizmoECS *gizmo)
{
    if (!gizmo || !gizmo->asset_drag.mouse_captured)
        return;

    if (gizmo->asset_drag.capture_input_system)
        gizmo->asset_drag.capture_input_system->EndMouseCapture(gizmo);

    gizmo->asset_drag.mouse_captured = false;
    gizmo->asset_drag.capture_input_system = nullptr;
}

static int GetScalePlaneNormalAxisFromEntry(const GizmoECS::AssetVisualPrimitive &entry)
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

