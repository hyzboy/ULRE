static hgl::ecs::Entity *CreateAssetVisualEntity(GizmoECS *gizmo,
                                                 hgl::ecs::Entity *parent,
                                                 const char *name,
                                                 const math::Vector3f &position,
                                                 const glm::quat &rotation,
                                                 const math::Vector3f &scale,
                                                 std::shared_ptr<hgl::ecs::TransformComponent> *out_transform = nullptr)
{
    if (!gizmo || !gizmo->world || !parent)
        return nullptr;

    auto *entity = gizmo->world->CreateEntity<hgl::ecs::Entity>(name ? name : "GizmoAssetVisual");
    if (!entity)
        return nullptr;

    auto transform = entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
    if (!transform)
        return nullptr;

    transform->SetLocalTRS(glm::vec3(position), rotation, glm::vec3(scale));
    transform->SetParent(parent->GetID());

    if (out_transform)
        *out_transform = transform;

    gizmo->asset_visual_entity_ids.push_back(entity->GetID());
    return entity;
}

static bool AttachAssetModePrimitive(std::vector<GizmoECS::AssetVisualPrimitive> &out_list,
                                     hgl::ecs::Entity *entity,
                                     const GizmoShape shape,
                                     const GizmoColor color,
                                     const int group_id = -1)
{
    if (!entity)
        return false;

    auto primitive = GetGizmoMeshPrimitive(shape);
    if (!primitive)
        return false;

    auto *base_material = GetGizmoMI3D(color);
    if (!base_material)
        return false;

    auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
    if (!prim_comp)
        return false;

    prim_comp->SetPrimitive(primitive);
    prim_comp->SetOverrideMaterial(base_material);
    prim_comp->SetVisible(false);

    GizmoECS::AssetVisualPrimitive item;
    item.primitive = prim_comp;
    item.transform = entity->GetComponent<hgl::ecs::TransformComponent>();
    item.base_material = base_material;
    item.shape = shape;
    item.group_id = group_id;
    out_list.push_back(item);
    return true;
}

static std::vector<GizmoECS::AssetVisualPrimitive> *GetActiveAssetVisualList(GizmoECS *gizmo)
{
    if (!gizmo)
        return nullptr;

    switch (gizmo->current_mode)
    {
    case GizmoMode::MoveWorld:
    case GizmoMode::MoveLocal:
        return &gizmo->move_primitives;
    case GizmoMode::RotateWorld:
    case GizmoMode::RotateLocal:
        return &gizmo->rotate_primitives;
    case GizmoMode::ScaleLocal:
        return &gizmo->scale_primitives;
    }

    return nullptr;
}

static void ApplyAssetVisualHighlightByIndex(GizmoECS *gizmo, int best_index)
{
    if (!gizmo)
        return;

    auto *items = GetActiveAssetVisualList(gizmo);
    if (!items)
    {
        gizmo->asset_hovered_visual_index = -1;
        gizmo->asset_visual_highlighted = false;
        return;
    }

    gizmo->asset_hovered_visual_index = best_index;

    const int best_group = (best_index >= 0 && best_index < static_cast<int>(items->size()))
                         ? (*items)[best_index].group_id
                         : -1;

    for (size_t i = 0; i < items->size(); ++i)
    {
        auto &entry = (*items)[i];
        if (!entry.primitive)
            continue;

        const bool in_group = (best_group >= 0 && entry.group_id == best_group);
        if (static_cast<int>(i) == best_index || in_group)
            entry.primitive->SetOverrideMaterial(GetGizmoMI3D(GizmoColor::Yellow));
        else
            entry.primitive->SetOverrideMaterial(entry.base_material);
    }

    gizmo->asset_visual_highlighted = (best_index >= 0);
}

static void SetAssetVisualHighlight(GizmoECS *gizmo, bool highlighted)
{
    if (!gizmo)
        return;

    auto apply = [highlighted](std::vector<GizmoECS::AssetVisualPrimitive> &items)
    {
        for (auto &entry : items)
        {
            if (!entry.primitive)
                continue;

            entry.primitive->SetOverrideMaterial(highlighted ? GetGizmoMI3D(GizmoColor::Yellow)
                                                             : entry.base_material);
        }
    };

    switch (gizmo->current_mode)
    {
    case GizmoMode::MoveWorld:
    case GizmoMode::MoveLocal:
        apply(gizmo->move_primitives);
        break;
    case GizmoMode::RotateWorld:
    case GizmoMode::RotateLocal:
        apply(gizmo->rotate_primitives);
        break;
    case GizmoMode::ScaleLocal:
        apply(gizmo->scale_primitives);
        break;
    }

    gizmo->asset_visual_highlighted = highlighted;
    gizmo->asset_hovered_visual_index = -1;
}

static void UpdateAssetVisualHover(GizmoECS *gizmo,
                                   const math::Vector2i &mouse_coord,
                                   const CameraInfo *camera_info,
                                   const ViewportInfo *viewport_info)
{
    if (!gizmo)
        return;

    auto *items = GetActiveAssetVisualList(gizmo);
    if (!items || items->empty() || !camera_info || !viewport_info)
        return;

    const math::Vector2u viewport_size = viewport_info->GetViewport();
    if (viewport_size.x == 0 || viewport_size.y == 0)
        return;

    constexpr float kPointHoverRadiusPx = 30.0f;
    constexpr float kSegmentHoverRadiusPx = 22.0f;
    constexpr float kRingHoverRadiusPx = 24.0f;

    int best_index = -1;
    int best_priority = 99;
    float best_score = 1e9f;

    if (!gizmo->root_transform)
        return;

    gizmo->root_transform->UpdateIfDirty();
    const math::Vector3f root_world_pos = gizmo->root_transform->GetWorldPosition();
    const math::Vector2i root_screen_pos = WorldPositionToScreen(root_world_pos, camera_info, viewport_size);
    const glm::vec2 mouse_pt(static_cast<float>(mouse_coord.x), static_cast<float>(mouse_coord.y));
    const float world_units_per_pixel = gizmo->root_transform->ComputeWorldUnitsPerPixel(camera_info, viewport_info);

    math::Ray mouse_ray;
    mouse_ray.SetFromViewportPoint(mouse_coord, camera_info, viewport_size);

    auto point_segment_distance = [](const glm::vec2 &p, const glm::vec2 &a, const glm::vec2 &b) -> float
    {
        const glm::vec2 ab = b - a;
        const float ab2 = glm::dot(ab, ab);
        if (ab2 <= 1e-6f)
            return glm::length(p - a);

        const float t = std::clamp(glm::dot(p - a, ab) / ab2, 0.0f, 1.0f);
        const glm::vec2 proj = a + ab * t;
        return glm::length(p - proj);
    };

    auto hover_priority = [](const GizmoShape shape) -> int
    {
        // Lower value means higher pick priority when overlapping on screen.
        switch (shape)
        {
        case GizmoShape::Cylinder:
        case GizmoShape::Cone:
        case GizmoShape::Cube:
        case GizmoShape::Torus:
            return 0; // Axis-driving components
        case GizmoShape::Square:
            return 2; // Plane handles
        case GizmoShape::Sphere:
            return 3; // Center handle
        default:
            return 1;
        }
    };

    for (size_t i = 0; i < items->size(); ++i)
    {
        auto &entry = (*items)[i];
        if (!entry.transform || !entry.primitive || !entry.primitive->IsVisible())
            continue;

        entry.transform->UpdateIfDirty();
        const math::Vector3f wp = entry.transform->GetWorldPosition();
        const math::Vector2i sp = WorldPositionToScreen(wp, camera_info, viewport_size);

        bool candidate = false;
        float score = 1e9f;

        if (entry.shape == GizmoShape::Torus)
        {
            const glm::vec3 center = wp;
            const glm::quat ring_rot = entry.transform->GetWorldRotation();
            const glm::vec3 ring_normal = glm::normalize(ring_rot * math::AxisVector::X);
            const float world_radius = std::max(entry.transform->GetWorldScale().x, 1e-3f);

            // Robust ring hit test: intersect mouse ray with ring plane and check
            // the radial distance error in world space. This remains accurate for
            // oblique rings (ellipse in screen space), not only front-facing rings.
            const float denom = glm::dot(mouse_ray.direction, ring_normal);
            if (std::fabs(denom) > 1e-6f)
            {
                const float t = glm::dot(center - mouse_ray.origin, ring_normal) / denom;
                if (t >= 0.0f)
                {
                    const math::Vector3f hit_point = mouse_ray.origin + mouse_ray.direction * t;
                    const float ring_error_world = std::fabs(glm::distance(hit_point, center) - world_radius);

                    const bool is_white_view_ring = (entry.group_id == 3);
                    const float legacy_ratio = is_white_view_ring ? (1.0f / 13.0f) : (0.5f / 10.0f);
                    float ring_threshold_world = world_radius * legacy_ratio;

                    if (world_units_per_pixel > 0.0f)
                        ring_threshold_world = std::max(ring_threshold_world, kRingHoverRadiusPx * world_units_per_pixel * 0.5f);

                    if (ring_error_world <= ring_threshold_world)
                    {
                        candidate = true;
                        score = ring_error_world / std::max(ring_threshold_world, 1e-5f);
                    }
                }
            }
        }
        else if (entry.shape == GizmoShape::Cylinder || entry.shape == GizmoShape::Cone)
        {
            const glm::vec2 a(static_cast<float>(root_screen_pos.x), static_cast<float>(root_screen_pos.y));
            // Use the actual far end of the shape (cylinder tip / cone tip) as segment endpoint
            // so the full visible length is hittable, not just up to the center.
            const math::Vector3f rot_z = entry.transform->GetWorldRotation() * glm::vec3(0.0f, 0.0f, 1.0f);
            const math::Vector3f far_wp = wp + rot_z * entry.transform->GetWorldScale().z;
            const math::Vector2i far_sp = WorldPositionToScreen(far_wp, camera_info, viewport_size);
            const glm::vec2 b(static_cast<float>(far_sp.x), static_cast<float>(far_sp.y));
            const float d = point_segment_distance(mouse_pt, a, b);
            if (d <= kSegmentHoverRadiusPx)
            {
                candidate = true;
                score = d;
            }
        }
        else
        {
            const glm::vec2 p(static_cast<float>(sp.x), static_cast<float>(sp.y));
            const float d = glm::length(mouse_pt - p);
            if (d <= kPointHoverRadiusPx)
            {
                candidate = true;
                score = d;
            }
        }

        if (candidate)
        {
            const int priority = hover_priority(entry.shape);
            if (priority < best_priority || (priority == best_priority && score <= best_score))
            {
                best_priority = priority;
                best_score = score;
                best_index = static_cast<int>(i);
            }
        }
    }

    ApplyAssetVisualHighlightByIndex(gizmo, best_index);
}

static void BuildMoveAssetVisual(GizmoECS *gizmo, hgl::ecs::Entity *parent)
{
    if (!gizmo || !parent)
        return;

    if (auto *center = CreateAssetVisualEntity(gizmo,
                                               parent,
                                               "GizmoAssetMove_Center",
                                               math::Vector3f(0.0f),
                                               glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                               math::Vector3f(1.0f) * kAssetVisualScale))
    {
        AttachAssetModePrimitive(gizmo->move_primitives, center, GizmoShape::Sphere, GizmoColor::White);
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
                                                "GizmoAssetMove_Cylinder",
                                                cfg.axis * GIZMO_CYLINDER_OFFSET * kAssetVisualScale,
                                                rotation,
                                                math::Vector3f(GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_RADIUS, GIZMO_CYLINDER_HALF_LENGTH) * kAssetVisualScale))
        {
            AttachAssetModePrimitive(gizmo->move_primitives, cyl, GizmoShape::Cylinder, cfg.color, i);
        }

        if (auto *cone = CreateAssetVisualEntity(gizmo,
                                                 parent,
                                                 "GizmoAssetMove_Cone",
                                                 cfg.axis * GIZMO_CONE_OFFSET * kAssetVisualScale,
                                                 rotation,
                                                 math::Vector3f(1.0f) * kAssetVisualScale))
        {
            AttachAssetModePrimitive(gizmo->move_primitives, cone, GizmoShape::Cone, cfg.color, i);
        }

        if (auto *plane = CreateAssetVisualEntity(gizmo,
                                                  parent,
                                                  "GizmoAssetMove_Plane",
                                                  cfg.plane_pos * kAssetVisualScale,
                                                  rotation,
                                                  math::Vector3f(2.0f) * kAssetVisualScale))
        {
            AttachAssetModePrimitive(gizmo->move_primitives, plane, GizmoShape::Square, cfg.color);
        }
    }
}

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
