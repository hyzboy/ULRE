static hgl::ecs::Entity *CreateAssetVisualEntity(GizmoECS *gizmo,
                                                  hgl::ecs::Entity *parent,
                                                  const char *name,
                                                  const math::Vector3f &position,
                                                  const glm::quat &rotation,
                                                  const math::Vector3f &scale,
                                                  std::shared_ptr<hgl::ecs::TransformComponent> *out_transform = nullptr)
{
    if (!gizmo || !parent)
        return nullptr;

    hgl::ecs::ECSContext::ChildEntityDesc desc;
    desc.name     = name ? name : "GizmoAssetVisual";
    desc.position = glm::vec3(position);
    desc.rotation = rotation;
    desc.scale    = glm::vec3(scale);
    return gizmo->world->CreateChildEntity(parent, desc, &gizmo->asset_visual_entity_ids, out_transform);
}

struct PrimitiveDesc
{
    const char        *name;
    math::Vector3f     pos;
    glm::quat          rot           = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    math::Vector3f     scale;
    GizmoShape         shape;
    GizmoColor         color;
    int                group_id      = -1;
    std::shared_ptr<hgl::ecs::TransformComponent> *out_transform = nullptr;
};

static bool AttachAssetModePrimitive(std::vector<GizmoVisualPrimitive> &out_list,
                                     hgl::ecs::Entity *entity,
                                     const GizmoShape shape,
                                     const GizmoColor color,
                                     const int group_id = -1)
{
    if (!entity)
        return false;

    auto primitive = GetGizmoMeshPrimitive(shape, color);
    if (!primitive)
        return false;

    auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
    if (!prim_comp)
        return false;

    prim_comp->SetPrimitive(primitive);
    prim_comp->SetVisible(false);

    GizmoVisualPrimitive item;
    item.primitive = prim_comp;
    item.transform = entity->GetComponent<hgl::ecs::TransformComponent>();
    item.base_color = color;
    item.shape = shape;
    item.group_id = group_id;
    out_list.push_back(item);
    return true;
}

static bool MakeAndAttachPrimitive(std::vector<GizmoVisualPrimitive> &primitives,
                                    hgl::ecs::ECSContext *world,
                                    hgl::ecs::Entity *parent,
                                    std::vector<hgl::ecs::EntityID> &entity_ids,
                                    const PrimitiveDesc &desc)
{
    hgl::ecs::ECSContext::ChildEntityDesc d;
    d.name = desc.name; d.position = glm::vec3(desc.pos); d.rotation = desc.rot; d.scale = glm::vec3(desc.scale);
    auto *entity = world->CreateChildEntity(parent, d, &entity_ids, desc.out_transform);
    if (!entity)
        return false;
    return AttachAssetModePrimitive(primitives, entity, desc.shape, desc.color, desc.group_id);
}

static void SetPrimitivesVisible(std::vector<GizmoVisualPrimitive> &primitives, bool visible)
{
    for (auto &entry : primitives)
    {
        if (entry.primitive)
            entry.primitive->SetVisible(visible);
    }
}

static std::vector<GizmoVisualPrimitive> *GetActiveAssetVisualList(GizmoECS *gizmo)
{
    if (!gizmo)
        return nullptr;

    if (IsMoveMode(gizmo->current_mode))
        return &gizmo->move_mode.primitives;
    if (IsRotateMode(gizmo->current_mode))
        return &gizmo->rotate_mode.primitives;
    return &gizmo->scale_mode.primitives;
}

static void SetAssetVisualHighlight(GizmoECS *gizmo, bool highlighted)
{
    if (!gizmo)
        return;

    auto apply = [highlighted](std::vector<GizmoVisualPrimitive> &items)
    {
        for (auto &entry : items)
        {
            if (!entry.primitive)
                continue;

            graph::Primitive *p = highlighted
                ? GetGizmoMeshPrimitive(entry.shape, GizmoColor::Yellow)
                : GetGizmoMeshPrimitive(entry.shape, entry.base_color);
            entry.primitive->SetPrimitive(p);
        }
    };

    if (auto *items = GetActiveAssetVisualList(gizmo))
        apply(*items);
}

// Base overload: takes root_transform explicitly — usable from mode methods.
static int PickBestAssetVisualIndex(const std::vector<GizmoVisualPrimitive> &items,
                                    const std::shared_ptr<hgl::ecs::TransformComponent> &root_transform,
                                    const GizmoFrameInput &input)
{
    const math::Vector2i &mouse_coord   = input.mouse_coord;
    const CameraInfo    *camera_info   = input.camera_info;
    const ViewportInfo  *viewport_info = input.viewport_info;
    if (!root_transform || items.empty() || !camera_info || !viewport_info)
        return -1;

    const math::Vector2u viewport_size = viewport_info->GetViewport();
    if (viewport_size.x == 0 || viewport_size.y == 0)
        return -1;

    constexpr float kPointHoverRadiusPx = 30.0f;
    constexpr float kSegmentHoverRadiusPx = 22.0f;
    constexpr float kRingHoverRadiusPx = 24.0f;

    int best_index = -1;
    int best_priority = 99;
    float best_score = 1e9f;

    root_transform->UpdateIfDirty();
    const math::Vector3f root_world_pos = root_transform->GetWorldPosition();
    const math::Vector2i root_screen_pos = WorldPositionToScreen(root_world_pos, camera_info, viewport_size);
    const glm::vec2 mouse_pt(static_cast<float>(mouse_coord.x), static_cast<float>(mouse_coord.y));
    const float world_units_per_pixel = root_transform->ComputeWorldUnitsPerPixel(camera_info, viewport_info);

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

    for (size_t i = 0; i < items.size(); ++i)
    {
        auto &entry = items[i];
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

    return best_index;
}

// GizmoECS convenience wrapper.
static int PickBestAssetVisualIndex(const std::vector<GizmoVisualPrimitive> &items,
                                    GizmoECS *gizmo,
                                    const GizmoFrameInput &input)
{
    if (!gizmo)
        return -1;
    return PickBestAssetVisualIndex(items, gizmo->root_transform, input);
}

