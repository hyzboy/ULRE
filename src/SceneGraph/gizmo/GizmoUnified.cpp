/*
 统一 Gizmo 架构 - 通过 SubWorldComponent 管理三个 Gizmo 子世界

 结构：
     Main World
         └── GizmoECS (root)
                ├── Move (SubWorld)
                ├── Rotate (SubWorld)
                └── Scale (SubWorld)
*/

#include"Gizmo.h"
#include"GizmoInternal.h"
#include"GizmoResource.h"
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/World.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/AssetWorldRegistry.h>
#include<hgl/ecs/components/AssetInstanceComponent.h>
#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/VisibilityComponent.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/AssetInstanceBridgeSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/io/event/KeyboardEvent.h>
#include<glm/gtc/quaternion.hpp>
#include<glm/geometric.hpp>
#include<cstdlib>
#include<cstring>
#include<iostream>
#include<vector>
#include<utility>

namespace hgl::graph{

namespace
{
    constexpr float kAssetVisualScale = 0.42f;
    constexpr hgl::ecs::AssetVersion kGizmoAssetWorldVersion = 1u;
    constexpr hgl::ecs::AssetWorldId kGizmoMoveAssetWorldId   = 0x47495A4D4F000001ull; // "GIZMO" + 1
    constexpr hgl::ecs::AssetWorldId kGizmoRotateAssetWorldId = 0x47495A4D4F000002ull; // "GIZMO" + 2
    constexpr hgl::ecs::AssetWorldId kGizmoScaleAssetWorldId  = 0x47495A4D4F000003ull; // "GIZMO" + 3
    constexpr uint64_t kGizmoMoveOverrideRef = 0x47495A4D4F4D4F56ull;     // "GIZMOMOV"
    constexpr uint64_t kGizmoRotateOverrideRef = 0x47495A4D4F524F54ull;   // "GIZMOROT"
    constexpr uint64_t kGizmoScaleOverrideRef = 0x47495A4D4F534341ull;    // "GIZMOSCA"

    hgl::ecs::AssetWorldDef MakeGizmoAssetWorldDef(hgl::ecs::AssetWorldId id, const char *name)
    {
        hgl::ecs::AssetWorldDef def{};
        def.id = id;
        def.version = kGizmoAssetWorldVersion;
        def.name = name ? name : "GizmoAsset";
        return def;
    }

    bool EnsureGizmoAssetWorldDefinitions(hgl::ecs::ECSContext *world)
    {
        if (!world)
            return false;

        auto bridge = world->GetSystem<hgl::ecs::AssetInstanceBridgeSystem>();
        if (!bridge)
        {
            std::cout << "[GizmoECS] Asset bridge not ready, skip gizmo asset definition publish" << std::endl;
            return false;
        }

        static hgl::ecs::AssetWorldRegistry s_gizmo_asset_registry;
        if (!bridge->GetRegistry())
            bridge->SetRegistry(&s_gizmo_asset_registry);

        auto *registry = bridge->GetRegistry();
        if (!registry)
        {
            std::cout << "[GizmoECS] Asset bridge has no registry, skip gizmo asset definition publish" << std::endl;
            return false;
        }

        const bool move_ok = registry->Register(MakeGizmoAssetWorldDef(kGizmoMoveAssetWorldId, "GizmoMove"));
        const bool rotate_ok = registry->Register(MakeGizmoAssetWorldDef(kGizmoRotateAssetWorldId, "GizmoRotate"));
        const bool scale_ok = registry->Register(MakeGizmoAssetWorldDef(kGizmoScaleAssetWorldId, "GizmoScale"));

        if (move_ok && rotate_ok && scale_ok)
            std::cout << "[GizmoECS] Published gizmo AssetWorld definitions (move/rotate/scale)" << std::endl;

        return move_ok && rotate_ok && scale_ok;
    }

    hgl::ecs::InstanceId ComposeGizmoInstanceId(const hgl::ecs::EntityID root_id, const uint8_t mode_tag)
    {
        // Keep ids deterministic per root + mode in current scene lifetime.
        const uint64_t base = (static_cast<uint64_t>(root_id.index) << 16) | static_cast<uint64_t>(root_id.generation);
        return (base << 8) | static_cast<uint64_t>(mode_tag);
    }

    std::shared_ptr<hgl::ecs::AssetInstanceComponent> AttachGizmoAssetInstance(hgl::ecs::Entity *entity,
                                                                                const hgl::ecs::AssetWorldId world_id,
                                                                                const hgl::ecs::InstanceId instance_id,
                                                                                const uint64_t override_ref)
    {
        if (!entity)
            return nullptr;

        auto asset_instance = entity->GetComponent<hgl::ecs::AssetInstanceComponent>();
        if (!asset_instance)
            asset_instance = entity->AddComponent<hgl::ecs::AssetInstanceComponent>();

        if (!asset_instance)
            return nullptr;

        asset_instance->SetAssetWorldID(world_id);
        asset_instance->SetInstanceID(instance_id);
        asset_instance->SetExpectedVersion(kGizmoAssetWorldVersion);
        asset_instance->SetFlags(1u); // render pass id 1 for gizmo overlay route

        hgl::ecs::AssetOverrideRef ref{};
        ref.payload_ref = override_ref;
        ref.revision = 1u;
        asset_instance->SetOverrideRef(ref);
        asset_instance->SetVisibilityMask(~0ull);

        return asset_instance;
    }
}

// Global resident state definition - declared in GizmoInternal.h
GizmoSystemResidentState g_gizmo_resident_state;

struct GizmoECS
{
    struct AssetVisualPrimitive
    {
        std::shared_ptr<hgl::ecs::PrimitiveComponent> primitive;
        std::shared_ptr<hgl::ecs::TransformComponent> transform;
        MaterialInstance *base_material = nullptr;
        GizmoShape shape = GizmoShape::Sphere;
        int group_id = -1; ///< axis group: cylinder+cone/cube of same axis share same id; -1 = ungrouped
    };

    enum class Backend : uint8_t
    {
        LegacySubWorld = 0,
        AssetWorldBridge = 1,
    };

    hgl::ecs::ECSContext* world = nullptr;
    hgl::ecs::Entity* root = nullptr;
    std::shared_ptr<hgl::ecs::TransformComponent> root_transform;
    Backend backend = Backend::LegacySubWorld;

    // 保存各个 Gizmo 的内部指针（内部实现使用）
    void* move_impl = nullptr;     // MoveGizmoImpl*
    void* rotate_impl = nullptr;   // RotateGizmoImpl*
    void* scale_impl = nullptr;    // ScaleGizmoImpl*

    // 对应的 SubWorld 和 Entity
    hgl::ecs::Entity* move_entity = nullptr;
    hgl::ecs::Entity* rotate_entity = nullptr;
    hgl::ecs::Entity* scale_entity = nullptr;

    std::shared_ptr<hgl::ecs::SubWorldComponent> move_subworld;
    std::shared_ptr<hgl::ecs::SubWorldComponent> rotate_subworld;
    std::shared_ptr<hgl::ecs::SubWorldComponent> scale_subworld;
    std::shared_ptr<hgl::ecs::AssetInstanceComponent> move_asset_instance;
    std::shared_ptr<hgl::ecs::AssetInstanceComponent> rotate_asset_instance;
    std::shared_ptr<hgl::ecs::AssetInstanceComponent> scale_asset_instance;
    std::vector<AssetVisualPrimitive> move_primitives;
    std::vector<AssetVisualPrimitive> rotate_primitives;
    std::vector<AssetVisualPrimitive> scale_primitives;
    std::vector<hgl::ecs::EntityID> asset_visual_entity_ids;
    std::shared_ptr<hgl::ecs::TransformComponent> rotate_white_ring_transform;
    uint32_t asset_mode_revision_counter = 1u;
    bool asset_visual_highlighted = false;
    int asset_hovered_visual_index = -1;

    hgl::ecs::World* move_world = nullptr;
    hgl::ecs::World* rotate_world = nullptr;
    hgl::ecs::World* scale_world = nullptr;

    float last_rotate_angle = 0.0f;
    float last_scale_value = 1.0f;
    float last_scale_value_u = 1.0f;
    float last_scale_value_v = 1.0f;
    float last_move_dist = 0.0f;
    int last_rotate_axis = -1;
    int last_scale_axis = -1;
    int last_move_axis = -1;

    // Asset backend minimal interaction state (GZ4 step2).
    bool asset_dragging = false;
    GizmoMode asset_drag_mode = GizmoMode::MoveWorld;
    int asset_drag_pick_index = -1;
    int asset_drag_pick_group = -1;
    int asset_drag_pick_plane_normal_axis = -1;
    GizmoShape asset_drag_pick_shape = GizmoShape::Sphere;
    bool asset_mouse_captured = false;
    hgl::ecs::InputSystem *asset_capture_input_system = nullptr;
    math::Vector2i asset_drag_start_mouse{0, 0};
    math::Vector3f asset_drag_start_position{0.0f, 0.0f, 0.0f};
    glm::quat asset_drag_start_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    math::Vector3f asset_drag_start_scale{1.0f, 1.0f, 1.0f};

    GizmoMode current_mode = GizmoMode::MoveWorld;
    bool allow_negative_scale = true;
    bool root_visible = true;

    hgl::ecs::Entity* target_entity = nullptr;
    GizmoChangedCallback on_changed;
};

static GizmoECS::Backend ResolveGizmoBackend()
{
    const char *env = std::getenv("ULRE_GIZMO_BACKEND");
    if (!env || !*env)
        return GizmoECS::Backend::AssetWorldBridge;

    // Planned values: "legacy" or "asset". Unknown values fallback safely.
    if (std::strcmp(env, "asset") == 0)
        return GizmoECS::Backend::AssetWorldBridge;

    return GizmoECS::Backend::LegacySubWorld;
}

static GizmoECS::Backend SelectSupportedBackend(hgl::ecs::ECSContext *world)
{
    const auto requested = ResolveGizmoBackend();
    if (requested == GizmoECS::Backend::AssetWorldBridge)
    {
        auto bridge = world ? world->GetSystem<hgl::ecs::AssetInstanceBridgeSystem>() : nullptr;
        if (!bridge && world)
            bridge = world->RegisterTickSystem<hgl::ecs::AssetInstanceBridgeSystem>();

        if (!bridge)
        {
            std::cout << "[GizmoECS] ULRE_GIZMO_BACKEND=asset requested, but bridge is unavailable; fallback to legacy" << std::endl;
            return GizmoECS::Backend::LegacySubWorld;
        }

        std::cout << "[GizmoECS] ULRE_GIZMO_BACKEND=asset enabled" << std::endl;
        return GizmoECS::Backend::AssetWorldBridge;
    }

    return GizmoECS::Backend::LegacySubWorld;
}

// Legacy internal entry points (implementation body kept unchanged).
GizmoECS *CreateGizmoECS(hgl::ecs::ECSContext *world,
                         const char *name,
                         const math::Vector3f &position);
void DestroyTransformGizmo(GizmoECS *gizmo);
void SetTransformGizmoMode(GizmoECS *gizmo, GizmoMode mode);
GizmoMode GetTransformGizmoMode(const GizmoECS *gizmo);
void SetTransformGizmoVisible(GizmoECS *gizmo, bool visible);
bool BindTransformGizmoTargetEntity(GizmoECS *gizmo, hgl::ecs::Entity *target_entity);
hgl::ecs::Entity *GetTransformGizmoTargetEntity(const GizmoECS *gizmo);
void SetTransformGizmoChangedCallback(GizmoECS *gizmo, GizmoChangedCallback callback);
void SetTransformGizmoAllowNegativeScale(GizmoECS *gizmo, bool enabled);
bool IsTransformGizmoAllowNegativeScale(const GizmoECS *gizmo);
void UpdateTransformGizmo(GizmoECS *gizmo,
                          const math::Vector2i &mouse_coord,
                          const CameraInfo *camera_info,
                          const ViewportInfo *viewport_info,
                          hgl::ecs::InputSystem *input_system,
                          bool left_down,
                          bool left_pressed,
                          bool left_released);

static void SyncAllSubGizmoTransforms(GizmoECS *gizmo);

static void SyncGizmoAssetModeBindings(GizmoECS *gizmo)
{
    if (!gizmo)
        return;

    const bool move_active = gizmo->root_visible && (gizmo->current_mode == GizmoMode::MoveWorld || gizmo->current_mode == GizmoMode::MoveLocal);
    const bool rotate_active = gizmo->root_visible && (gizmo->current_mode == GizmoMode::RotateWorld || gizmo->current_mode == GizmoMode::RotateLocal);
    const bool scale_active = gizmo->root_visible && (gizmo->current_mode == GizmoMode::ScaleLocal);

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

    apply_active(gizmo->move_asset_instance, move_active, kGizmoMoveOverrideRef);
    apply_active(gizmo->rotate_asset_instance, rotate_active, kGizmoRotateOverrideRef);
    apply_active(gizmo->scale_asset_instance, scale_active, kGizmoScaleOverrideRef);

    for (auto &entry : gizmo->move_primitives)
    {
        if (entry.primitive)
            entry.primitive->SetVisible(move_active);
    }

    for (auto &entry : gizmo->rotate_primitives)
    {
        if (entry.primitive)
            entry.primitive->SetVisible(rotate_active);
    }

    for (auto &entry : gizmo->scale_primitives)
    {
        if (entry.primitive)
            entry.primitive->SetVisible(scale_active);
    }
}

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

static bool BeginAssetMouseCapture(GizmoECS *gizmo, hgl::ecs::InputSystem *input_system)
{
    if (!gizmo)
        return false;

    if (!input_system)
        return true;

    if (gizmo->asset_mouse_captured)
    {
        if (gizmo->asset_capture_input_system == input_system)
            return true;

        if (gizmo->asset_capture_input_system)
            gizmo->asset_capture_input_system->EndMouseCapture(gizmo);

        gizmo->asset_mouse_captured = false;
        gizmo->asset_capture_input_system = nullptr;
    }

    if (!input_system->BeginMouseCapture(gizmo))
        return false;

    gizmo->asset_mouse_captured = true;
    gizmo->asset_capture_input_system = input_system;
    return true;
}

static void EndAssetMouseCapture(GizmoECS *gizmo)
{
    if (!gizmo || !gizmo->asset_mouse_captured)
        return;

    if (gizmo->asset_capture_input_system)
        gizmo->asset_capture_input_system->EndMouseCapture(gizmo);

    gizmo->asset_mouse_captured = false;
    gizmo->asset_capture_input_system = nullptr;
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
            glm::vec3 ring_normal = glm::normalize(ring_rot * math::AxisVector::X);
            glm::vec3 camera_forward = glm::normalize(math::Vector3f(camera_info->view[0][2],
                                                                      camera_info->view[1][2],
                                                                      camera_info->view[2][2]));

            glm::vec3 tangent = glm::cross(ring_normal, camera_forward);
            if (glm::length(tangent) < 1e-4f)
                tangent = glm::cross(ring_normal, math::AxisVector::Y);
            if (glm::length(tangent) < 1e-4f)
                tangent = math::AxisVector::Z;
            tangent = glm::normalize(tangent);

            const float world_radius = std::max(entry.transform->GetWorldScale().x, 1e-3f);
            const math::Vector3f edge_world = center + tangent * world_radius;
            const math::Vector2i edge_sp = WorldPositionToScreen(edge_world, camera_info, viewport_size);

            const glm::vec2 center_pt(static_cast<float>(sp.x), static_cast<float>(sp.y));
            const glm::vec2 edge_pt(static_cast<float>(edge_sp.x), static_cast<float>(edge_sp.y));
            const float ring_radius_px = glm::length(edge_pt - center_pt);
            const float d = std::fabs(glm::length(mouse_pt - center_pt) - ring_radius_px);

            if (d <= kRingHoverRadiusPx)
            {
                candidate = true;
                score = d;
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

static bool IsNearlyEqual(const math::Vector3f &a, const math::Vector3f &b, float epsilon = 1e-5f)
{
    return glm::length(a - b) <= epsilon;
}

static bool IsNearlyEqualRotation(const glm::quat &a, const glm::quat &b, float epsilon = 1e-5f)
{
    const float d = std::fabs(glm::dot(a, b));
    return std::fabs(1.0f - d) <= epsilon;
}

static bool IsTransformChanged(const math::Vector3f &prev_pos,
                               const glm::quat &prev_rot,
                               const math::Vector3f &prev_scale,
                               const math::Vector3f &cur_pos,
                               const glm::quat &cur_rot,
                               const math::Vector3f &cur_scale)
{
    if(!IsNearlyEqual(prev_pos, cur_pos))
        return true;

    if(!IsNearlyEqualRotation(prev_rot, cur_rot))
        return true;

    if(!IsNearlyEqual(prev_scale, cur_scale))
        return true;

    return false;
}

static void NormalizeScaleByPolicy(glm::vec3 &scale, bool allow_negative_scale)
{
    if (!allow_negative_scale)
        scale = glm::abs(scale);

    auto clamp_component = [](float value)
    {
        if (std::fabs(value) < 0.05f)
            return (value < 0.0f) ? -0.05f : 0.05f;
        return value;
    };

    scale.x = clamp_component(scale.x);
    scale.y = clamp_component(scale.y);
    scale.z = clamp_component(scale.z);
}

static void ApplyScalePolicyToTargetIfNeeded(GizmoECS *gizmo)
{
    if (!gizmo || !gizmo->root_transform)
        return;

    glm::vec3 scale = gizmo->root_transform->GetLocalScale();
    const glm::vec3 original_scale = scale;
    NormalizeScaleByPolicy(scale, gizmo->allow_negative_scale);

    if (glm::length(scale - original_scale) > 1e-6f)
    {
        gizmo->root_transform->SetLocalScale(scale);

        if (gizmo->target_entity)
        {
            auto target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();
            if (target_transform)
                target_transform->SetLocalScale(scale);
        }

        SyncAllSubGizmoTransforms(gizmo);
    }
}

static void SyncAllSubGizmoTransforms(GizmoECS *gizmo)
{
    if(!gizmo || !gizmo->root_transform)
        return;

    if(gizmo->backend != GizmoECS::Backend::LegacySubWorld)
        return;

    const math::Vector3f root_pos = gizmo->root_transform->GetLocalPosition();
    const glm::quat root_rot = gizmo->root_transform->GetLocalRotation();

    SetMoveGizmoPosition((MoveGizmoImpl*)gizmo->move_impl, root_pos);
    SetMoveGizmoRotation((MoveGizmoImpl*)gizmo->move_impl,
                         gizmo->current_mode == GizmoMode::MoveLocal ? root_rot : glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    SetRotateGizmoPosition((RotateGizmoImpl*)gizmo->rotate_impl, root_pos);
    SetRotateGizmoRotation((RotateGizmoImpl*)gizmo->rotate_impl,
                          gizmo->current_mode == GizmoMode::RotateLocal ? root_rot : glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    SetScaleGizmoPosition((ScaleGizmoImpl*)gizmo->scale_impl, root_pos);
    SetScaleGizmoRotation((ScaleGizmoImpl*)gizmo->scale_impl, root_rot);
}

static bool IsCurrentModeDragging(const GizmoECS *gizmo)
{
    if(!gizmo)
        return false;

    if(gizmo->backend != GizmoECS::Backend::LegacySubWorld)
        return false;

    switch(gizmo->current_mode)
    {
    case GizmoMode::MoveWorld:
    case GizmoMode::MoveLocal:
        {
            MoveGizmoInteractionState state;
            return GetMoveGizmoInteractionState((const MoveGizmoImpl*)gizmo->move_impl, state) && state.dragging;
        }
    case GizmoMode::RotateWorld:
    case GizmoMode::RotateLocal:
        {
            RotateGizmoInteractionState state;
            return GetRotateGizmoInteractionState((const RotateGizmoImpl*)gizmo->rotate_impl, state) && state.dragging;
        }
    case GizmoMode::ScaleLocal:
        {
            ScaleGizmoInteractionState state;
            return GetScaleGizmoInteractionState((const ScaleGizmoImpl*)gizmo->scale_impl, state) && state.dragging;
        }
    }

    return false;
}


GizmoECS *CreateTransformGizmo(hgl::ecs::ECSContext *world,
                               const char *name,
                               const math::Vector3f &position)
{
    if (!world)
        return nullptr;

    auto *gizmo = new GizmoECS;
    gizmo->world = world;
    gizmo->backend = SelectSupportedBackend(world);
    EnsureGizmoAssetWorldDefinitions(world);
    std::cout << "[GizmoECS] Create begin name=" << (name ? name : "Gizmo") << std::endl;

    // Create root entity for entire Gizmo
    gizmo->root = world->CreateEntity<hgl::ecs::Entity>(name ? name : "Gizmo");
    if (!gizmo->root)
    {
        std::cout << "[GizmoECS] Create root entity failed" << std::endl;
        delete gizmo;
        return nullptr;
    }
    std::cout << "[GizmoECS] Root entity id=" << gizmo->root->GetID().index << std::endl;

    gizmo->root_transform = gizmo->root->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
    gizmo->root_transform->SetLocalTRS(glm::vec3(position), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    gizmo->root_transform->SetMovable(true);

    // Create three child entities for each Gizmo mode.

    // Move Gizmo
    {
        gizmo->move_entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Move");
        if (!gizmo->move_entity)
        {
            std::cout << "[GizmoECS] Create move entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto move_transform = gizmo->move_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        move_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        move_transform->SetParent(gizmo->root->GetID());
        if (gizmo->backend == GizmoECS::Backend::AssetWorldBridge)
        {
            move_transform->SetFixedPixelSizingParameters(GIZMO_FIXED_PIXEL_DIAMETER,
                                                          GIZMO_ARROW_LENGTH * 2.0f,
                                                          0.01f);
            move_transform->SetFixedPixelSizingEnabled(true);
        }

        if (gizmo->backend == GizmoECS::Backend::LegacySubWorld)
        {
            auto sub_world = gizmo->move_entity->AddComponent<hgl::ecs::SubWorldComponent>(hgl::ecs::SubWorldMode::IsolatedContext);
            gizmo->move_subworld = sub_world;
            gizmo->move_world = sub_world->GetSubWorld();

            if (!gizmo->move_world)
            {
                std::cout << "[GizmoECS] Move subworld is null" << std::endl;
                DestroyTransformGizmo(gizmo);
                return nullptr;
            }

            gizmo->move_impl = (void*)CreateMoveGizmoImpl(gizmo->move_world, "GizmoMove", math::Vector3f(0, 0, 0));
            if (!gizmo->move_impl)
            {
                std::cout << "[GizmoECS] Create move gizmo failed" << std::endl;
                DestroyTransformGizmo(gizmo);
                return nullptr;
            }
        }

        gizmo->move_asset_instance = AttachGizmoAssetInstance(gizmo->move_entity,
                                                               kGizmoMoveAssetWorldId,
                                                               ComposeGizmoInstanceId(gizmo->root->GetID(), 1u),
                                                               kGizmoMoveOverrideRef);

        if (gizmo->backend == GizmoECS::Backend::AssetWorldBridge)
            BuildMoveAssetVisual(gizmo, gizmo->move_entity);
    }

    // Rotate Gizmo
    {
        gizmo->rotate_entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Rotate");
        if (!gizmo->rotate_entity)
        {
            std::cout << "[GizmoECS] Create rotate entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto rotate_transform = gizmo->rotate_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        rotate_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        rotate_transform->SetParent(gizmo->root->GetID());
        if (gizmo->backend == GizmoECS::Backend::AssetWorldBridge)
        {
            rotate_transform->SetFixedPixelSizingParameters(GIZMO_FIXED_PIXEL_DIAMETER,
                                                            GIZMO_ARROW_LENGTH * 2.0f,
                                                            0.01f);
            rotate_transform->SetFixedPixelSizingEnabled(true);
        }

        if (gizmo->backend == GizmoECS::Backend::LegacySubWorld)
        {
            auto sub_world = gizmo->rotate_entity->AddComponent<hgl::ecs::SubWorldComponent>(hgl::ecs::SubWorldMode::IsolatedContext);
            gizmo->rotate_subworld = sub_world;
            gizmo->rotate_world = sub_world->GetSubWorld();

            if (!gizmo->rotate_world)
            {
                std::cout << "[GizmoECS] Rotate subworld is null" << std::endl;
                DestroyTransformGizmo(gizmo);
                return nullptr;
            }

            gizmo->rotate_impl = (void*)CreateRotateGizmoImpl(gizmo->rotate_world, "GizmoRotate", math::Vector3f(0, 0, 0));
            if (!gizmo->rotate_impl)
            {
                std::cout << "[GizmoECS] Create rotate gizmo failed" << std::endl;
                DestroyTransformGizmo(gizmo);
                return nullptr;
            }
        }

        gizmo->rotate_asset_instance = AttachGizmoAssetInstance(gizmo->rotate_entity,
                                                                 kGizmoRotateAssetWorldId,
                                                                 ComposeGizmoInstanceId(gizmo->root->GetID(), 2u),
                                                                 kGizmoRotateOverrideRef);

        if (gizmo->backend == GizmoECS::Backend::AssetWorldBridge)
            BuildRotateAssetVisual(gizmo, gizmo->rotate_entity);
    }

    // Scale Gizmo
    {
        gizmo->scale_entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Scale");
        if (!gizmo->scale_entity)
        {
            std::cout << "[GizmoECS] Create scale entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto scale_transform = gizmo->scale_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        scale_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        scale_transform->SetParent(gizmo->root->GetID());
        if (gizmo->backend == GizmoECS::Backend::AssetWorldBridge)
        {
            scale_transform->SetFixedPixelSizingParameters(GIZMO_FIXED_PIXEL_DIAMETER,
                                                           GIZMO_ARROW_LENGTH * 2.0f,
                                                           0.01f);
            scale_transform->SetFixedPixelSizingEnabled(true);
        }

        if (gizmo->backend == GizmoECS::Backend::LegacySubWorld)
        {
            auto sub_world = gizmo->scale_entity->AddComponent<hgl::ecs::SubWorldComponent>(hgl::ecs::SubWorldMode::IsolatedContext);
            gizmo->scale_subworld = sub_world;
            gizmo->scale_world = sub_world->GetSubWorld();

            if (!gizmo->scale_world)
            {
                std::cout << "[GizmoECS] Scale subworld is null" << std::endl;
                DestroyTransformGizmo(gizmo);
                return nullptr;
            }

            gizmo->scale_impl = (void*)CreateScaleGizmoImpl(gizmo->scale_world, "GizmoScale", math::Vector3f(0, 0, 0));
            if (!gizmo->scale_impl)
            {
                std::cout << "[GizmoECS] Create scale gizmo failed" << std::endl;
                DestroyTransformGizmo(gizmo);
                return nullptr;
            }
        }

        gizmo->scale_asset_instance = AttachGizmoAssetInstance(gizmo->scale_entity,
                                                                kGizmoScaleAssetWorldId,
                                                                ComposeGizmoInstanceId(gizmo->root->GetID(), 3u),
                                                                kGizmoScaleOverrideRef);

        if (gizmo->backend == GizmoECS::Backend::AssetWorldBridge)
            BuildScaleAssetVisual(gizmo, gizmo->scale_entity);
    }

    // Initialize with Move mode active
    SetTransformGizmoMode(gizmo, GizmoMode::MoveWorld);
    SyncAllSubGizmoTransforms(gizmo);
    std::cout << "[GizmoECS] Create done" << std::endl;

    return gizmo;
}

GizmoECS *CreateDefaultTransformGizmo(hgl::ecs::ECSContext *world,
                                      const char *name,
                                      const math::Vector3f &position,
                                      GizmoMode default_mode)
{
    GizmoECS *gizmo = CreateTransformGizmo(world, name, position);
    if(!gizmo)
        return nullptr;

    SetTransformGizmoMode(gizmo, default_mode);
    SetTransformGizmoVisible(gizmo, true);
    return gizmo;
}

void DestroyTransformGizmo(GizmoECS *gizmo)
{
    if (!gizmo)
        return;

    std::cout << "[GizmoECS] Destroy begin" << std::endl;

    EndAssetMouseCapture(gizmo);

    if (gizmo->move_impl)
    {
        DestroyMoveGizmoImpl((MoveGizmoImpl*)gizmo->move_impl);
    }

    if (gizmo->rotate_impl)
    {
        DestroyRotateGizmoImpl((RotateGizmoImpl*)gizmo->rotate_impl);
    }

    if (gizmo->scale_impl)
    {
        DestroyScaleGizmoImpl((ScaleGizmoImpl*)gizmo->scale_impl);
    }

    if (gizmo->world)
    {
        for (const auto &id : gizmo->asset_visual_entity_ids)
        {
            if (id.IsValid())
                gizmo->world->DestroyEntity(id);
        }

        if (gizmo->move_entity)
            gizmo->world->DestroyEntity(gizmo->move_entity->GetID());
        if (gizmo->rotate_entity)
            gizmo->world->DestroyEntity(gizmo->rotate_entity->GetID());
        if (gizmo->scale_entity)
            gizmo->world->DestroyEntity(gizmo->scale_entity->GetID());
        if (gizmo->root)
            gizmo->world->DestroyEntity(gizmo->root->GetID());
    }

    delete gizmo;
    std::cout << "[GizmoECS] Destroy done" << std::endl;
}

void SetTransformGizmoMode(GizmoECS *gizmo, GizmoMode mode)
{
    if (!gizmo)
        return;

    gizmo->current_mode = mode;
    std::cout << "[GizmoECS] Set mode=" << static_cast<int>(mode) << std::endl;

    // Set visibility based on mode
    const bool move_active = (mode == GizmoMode::MoveWorld || mode == GizmoMode::MoveLocal);
    const bool rotate_active = (mode == GizmoMode::RotateWorld || mode == GizmoMode::RotateLocal);

    if (gizmo->backend == GizmoECS::Backend::LegacySubWorld)
    {
        SetMoveGizmoVisible((MoveGizmoImpl*)gizmo->move_impl, move_active);
        SetRotateGizmoVisible((RotateGizmoImpl*)gizmo->rotate_impl, rotate_active);
        SetScaleGizmoVisible((ScaleGizmoImpl*)gizmo->scale_impl, mode == GizmoMode::ScaleLocal);
    }

    // Pause non-active sub-worlds to avoid concurrent rendering/update artifacts
    if (gizmo->move_subworld)
        gizmo->move_subworld->SetPaused(!move_active);
    if (gizmo->rotate_subworld)
        gizmo->rotate_subworld->SetPaused(!rotate_active);
    if (gizmo->scale_subworld)
        gizmo->scale_subworld->SetPaused(mode != GizmoMode::ScaleLocal);

    SyncGizmoAssetModeBindings(gizmo);
    EndAssetMouseCapture(gizmo);
    gizmo->asset_dragging = false;
    gizmo->asset_drag_pick_index = -1;
    gizmo->asset_drag_pick_group = -1;
    gizmo->asset_drag_pick_plane_normal_axis = -1;
    SetAssetVisualHighlight(gizmo, false);
    gizmo->asset_hovered_visual_index = -1;

    gizmo->last_rotate_angle = 0.0f;
    gizmo->last_scale_value = 1.0f;
    gizmo->last_scale_value_u = 1.0f;
    gizmo->last_scale_value_v = 1.0f;
    gizmo->last_move_dist = 0.0f;
    gizmo->last_rotate_axis = -1;
    gizmo->last_scale_axis = -1;
    gizmo->last_move_axis = -1;

    SyncAllSubGizmoTransforms(gizmo);
}

GizmoMode GetTransformGizmoMode(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->current_mode : GizmoMode::MoveWorld;
}

void SetTransformGizmoVisible(GizmoECS *gizmo, bool visible)
{
    if (!gizmo || !gizmo->root)
        return;

    gizmo->root_visible = visible;

    auto vis_comp = gizmo->root->GetComponent<hgl::ecs::VisibilityComponent>();
    if (!vis_comp)
    {
        vis_comp = gizmo->root->AddComponent<hgl::ecs::VisibilityComponent>();
    }

    if (vis_comp)
    {
        vis_comp->SetVisible(visible);
        std::cout << "[GizmoECS] Set root visible=" << (visible ? 1 : 0) << std::endl;
    }

    SyncGizmoAssetModeBindings(gizmo);
}

hgl::ecs::Entity *GetGizmoRootEntity(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->root : nullptr;
}

bool BindTransformGizmoTargetEntity(GizmoECS *gizmo, hgl::ecs::Entity *target_entity)
{
    if(!gizmo)
        return false;

    gizmo->target_entity = target_entity;

    if(!target_entity || !gizmo->root_transform)
        return true;

    auto target_transform = target_entity->GetComponent<hgl::ecs::TransformComponent>();
    if(!target_transform)
        return false;

    gizmo->root_transform->SetLocalTRS(target_transform->GetLocalPosition(),
                                       target_transform->GetLocalRotation(),
                                       target_transform->GetLocalScale());
    SyncAllSubGizmoTransforms(gizmo);

    return true;
}

hgl::ecs::Entity *GetTransformGizmoTargetEntity(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->target_entity : nullptr;
}

void SetTransformGizmoChangedCallback(GizmoECS *gizmo, GizmoChangedCallback callback)
{
    if(!gizmo)
        return;

    gizmo->on_changed = std::move(callback);
}

void SetTransformGizmoAllowNegativeScale(GizmoECS *gizmo, bool enabled)
{
    if (!gizmo)
        return;

    gizmo->allow_negative_scale = enabled;
    ApplyScalePolicyToTargetIfNeeded(gizmo);
}

bool IsTransformGizmoAllowNegativeScale(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->allow_negative_scale : true;
}

void UpdateTransformGizmo(GizmoECS *gizmo,
                          const math::Vector2i &mouse_coord,
                          const CameraInfo *camera_info,
                          const ViewportInfo *viewport_info,
                    hgl::ecs::InputSystem *input_system,
                    bool left_down,
                    bool left_pressed,
                    bool left_released)
{
    if (!gizmo)
        return;

    if (gizmo->backend != GizmoECS::Backend::LegacySubWorld)
    {
        if (!gizmo->root_transform)
            return;

        if (gizmo->target_entity && !gizmo->asset_dragging)
        {
            auto target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();
            if (target_transform)
                gizmo->root_transform->SetLocalTRS(target_transform->GetLocalPosition(),
                                                   target_transform->GetLocalRotation(),
                                                   target_transform->GetLocalScale());
        }

        const math::Vector3f prev_pos = gizmo->root_transform->GetLocalPosition();
        const glm::quat prev_rot = gizmo->root_transform->GetLocalRotation();
        const math::Vector3f prev_scale = gizmo->root_transform->GetLocalScale();

        if (!gizmo->asset_dragging)
        {
            if (gizmo->root_visible)
                UpdateAssetVisualHover(gizmo, mouse_coord, camera_info, viewport_info);
            else
                SetAssetVisualHighlight(gizmo, false);
        }

        if (left_pressed && !gizmo->asset_dragging)
        {
            if (gizmo->root_visible)
                UpdateAssetVisualHover(gizmo, mouse_coord, camera_info, viewport_info);

            const int picked = gizmo->asset_hovered_visual_index;
            auto *items = GetActiveAssetVisualList(gizmo);
            const bool has_view_context = (camera_info && viewport_info);

            if (gizmo->root_visible && items && picked >= 0 && picked < static_cast<int>(items->size()))
            {
                if (!BeginAssetMouseCapture(gizmo, input_system))
                    return;

                const auto &picked_entry = (*items)[picked];
                gizmo->asset_dragging = true;
                gizmo->asset_drag_mode = gizmo->current_mode;
                gizmo->asset_drag_pick_index = picked;
                gizmo->asset_drag_pick_group = picked_entry.group_id;
                gizmo->asset_drag_pick_shape = picked_entry.shape;
                gizmo->asset_drag_pick_plane_normal_axis = GetScalePlaneNormalAxisFromEntry(picked_entry);
                gizmo->asset_drag_start_mouse = mouse_coord;
                gizmo->asset_drag_start_position = prev_pos;
                gizmo->asset_drag_start_rotation = prev_rot;
                gizmo->asset_drag_start_scale = prev_scale;
                ApplyAssetVisualHighlightByIndex(gizmo, picked);
            }
            else if (!has_view_context)
            {
                // Keep deterministic headless/no-camera behavior used by smoke tests.
                if (!BeginAssetMouseCapture(gizmo, input_system))
                    return;

                gizmo->asset_dragging = true;
                gizmo->asset_drag_mode = gizmo->current_mode;
                gizmo->asset_drag_pick_index = -1;
                gizmo->asset_drag_pick_group = -1;
                gizmo->asset_drag_pick_plane_normal_axis = -1;
                gizmo->asset_drag_start_mouse = mouse_coord;
                gizmo->asset_drag_start_position = prev_pos;
                gizmo->asset_drag_start_rotation = prev_rot;
                gizmo->asset_drag_start_scale = prev_scale;
            }
        }

        if (left_released)
        {
            gizmo->asset_dragging = false;
            gizmo->asset_drag_pick_index = -1;
            gizmo->asset_drag_pick_group = -1;
            gizmo->asset_drag_pick_plane_normal_axis = -1;
            EndAssetMouseCapture(gizmo);

            if (gizmo->root_visible)
                UpdateAssetVisualHover(gizmo, mouse_coord, camera_info, viewport_info);
            else
                SetAssetVisualHighlight(gizmo, false);
        }

            if (gizmo->asset_dragging && !left_down && !left_released)
            {
                // Robust fallback when button-up edge is missed by caller.
                gizmo->asset_dragging = false;
                gizmo->asset_drag_pick_index = -1;
                gizmo->asset_drag_pick_group = -1;
                gizmo->asset_drag_pick_plane_normal_axis = -1;
                EndAssetMouseCapture(gizmo);
            }

        if (gizmo->asset_dragging && left_down)
        {
            const float dx = static_cast<float>(mouse_coord.x - gizmo->asset_drag_start_mouse.x);
            const float dy = static_cast<float>(mouse_coord.y - gizmo->asset_drag_start_mouse.y);

            constexpr float kMoveSensitivity = 0.01f;
            constexpr float kRotateSensitivity = 0.005f;
            constexpr float kScaleSensitivity = 0.01f;

            math::Vector3f camera_right = math::AxisVector::X;
            math::Vector3f camera_up = math::AxisVector::Y;
            if (camera_info)
            {
                camera_right = glm::normalize(math::Vector3f(camera_info->view[0][0], camera_info->view[1][0], camera_info->view[2][0]));
                camera_up = glm::normalize(math::Vector3f(camera_info->view[0][1], camera_info->view[1][1], camera_info->view[2][1]));
            }

            auto axis_from_index = [&](int axis_index, bool local_space) -> glm::vec3
            {
                glm::vec3 axis = math::GetAxisVector(math::AXIS(axis_index));
                if (local_space)
                    axis = gizmo->asset_drag_start_rotation * axis;
                return glm::normalize(axis);
            };

            auto plane_axes_from_normal = [](int normal_axis, int &u_axis, int &v_axis)
            {
                switch (normal_axis)
                {
                case 0: u_axis = 1; v_axis = 2; break; // YZ plane
                case 1: u_axis = 0; v_axis = 2; break; // XZ plane
                case 2: u_axis = 0; v_axis = 1; break; // XY plane
                default: u_axis = 0; v_axis = 1; break;
                }
            };

            auto project_mouse_delta_to_axis_pixels = [&](const glm::vec3 &axis_world) -> float
            {
                if (!camera_info || !viewport_info)
                    return 0.0f;

                const math::Vector2u viewport_size = viewport_info->GetViewport();
                if (viewport_size.x == 0 || viewport_size.y == 0)
                    return 0.0f;

                const math::Vector3f p0 = gizmo->asset_drag_start_position;
                const math::Vector3f p1 = p0 + axis_world * (GIZMO_ARROW_LENGTH * kAssetVisualScale);
                const math::Vector2i s0 = WorldPositionToScreen(p0, camera_info, viewport_size);
                const math::Vector2i s1 = WorldPositionToScreen(p1, camera_info, viewport_size);

                glm::vec2 dir(static_cast<float>(s1.x - s0.x), static_cast<float>(s1.y - s0.y));
                const float len = glm::length(dir);
                if (len < 1e-4f)
                    return 0.0f;

                dir /= len;
                const glm::vec2 mouse_delta(static_cast<float>(mouse_coord.x - gizmo->asset_drag_start_mouse.x),
                                            static_cast<float>(mouse_coord.y - gizmo->asset_drag_start_mouse.y));
                return glm::dot(mouse_delta, dir);
            };

            auto compute_rotation_delta = [&]() -> float
            {
                if (!camera_info || !viewport_info)
                    return (-dx - dy) * kRotateSensitivity;

                const math::Vector2u viewport_size = viewport_info->GetViewport();
                if (viewport_size.x == 0 || viewport_size.y == 0)
                    return (-dx - dy) * kRotateSensitivity;

                const math::Vector2i center = WorldPositionToScreen(gizmo->asset_drag_start_position, camera_info, viewport_size);
                const glm::vec2 c(static_cast<float>(center.x), static_cast<float>(center.y));
                const glm::vec2 v0(static_cast<float>(gizmo->asset_drag_start_mouse.x) - c.x,
                                   static_cast<float>(gizmo->asset_drag_start_mouse.y) - c.y);
                const glm::vec2 v1(static_cast<float>(mouse_coord.x) - c.x,
                                   static_cast<float>(mouse_coord.y) - c.y);

                if (glm::length(v0) < 4.0f || glm::length(v1) < 4.0f)
                    return (-dx - dy) * kRotateSensitivity;

                const float cross_z = v0.x * v1.y - v0.y * v1.x;
                const float dot_v = glm::dot(v0, v1);
                return std::atan2(cross_z, dot_v);
            };

            switch (gizmo->asset_drag_mode)
            {
            case GizmoMode::MoveWorld:
            case GizmoMode::MoveLocal:
                {
                    if (gizmo->asset_drag_pick_group >= 0 && gizmo->asset_drag_pick_group < 3)
                    {
                        const bool local_space = (gizmo->asset_drag_mode == GizmoMode::MoveLocal);
                        const glm::vec3 move_axis = axis_from_index(gizmo->asset_drag_pick_group, local_space);
                        float delta_world = project_mouse_delta_to_axis_pixels(move_axis);

                        if (camera_info && viewport_info)
                        {
                            const float wupp = gizmo->root_transform->ComputeWorldUnitsPerPixel(camera_info, viewport_info);
                            if (wupp > 0.0f)
                                delta_world *= wupp;
                            else
                                delta_world *= kMoveSensitivity;
                        }
                        else
                        {
                            delta_world *= kMoveSensitivity;
                        }

                        gizmo->root_transform->SetLocalPosition(gizmo->asset_drag_start_position + move_axis * delta_world);
                    }
                    else if (gizmo->asset_drag_pick_shape == GizmoShape::Square &&
                             gizmo->asset_drag_pick_plane_normal_axis >= 0 &&
                             camera_info && viewport_info)
                    {
                        // Map screen-space mouse delta to world-space movement on the selected 2-axis plane.
                        // This keeps both direction and distance consistent with pointer motion.
                        const bool local_space = (gizmo->asset_drag_mode == GizmoMode::MoveLocal);
                        int u_axis = 0;
                        int v_axis = 1;
                        plane_axes_from_normal(gizmo->asset_drag_pick_plane_normal_axis, u_axis, v_axis);

                        const glm::vec3 u_world = axis_from_index(u_axis, local_space);
                        const glm::vec3 v_world = axis_from_index(v_axis, local_space);

                        const math::Vector2u viewport_size = viewport_info->GetViewport();
                        if (viewport_size.x > 0 && viewport_size.y > 0)
                        {
                            const float ref_len = std::max(0.1f, GIZMO_TWO_AXIS_OFFSET * kAssetVisualScale);
                            const math::Vector3f p0 = gizmo->asset_drag_start_position;
                            const math::Vector3f pu = p0 + u_world * ref_len;
                            const math::Vector3f pv = p0 + v_world * ref_len;

                            const math::Vector2i s0 = WorldPositionToScreen(p0, camera_info, viewport_size);
                            const math::Vector2i su = WorldPositionToScreen(pu, camera_info, viewport_size);
                            const math::Vector2i sv = WorldPositionToScreen(pv, camera_info, viewport_size);

                            const glm::vec2 du(static_cast<float>(su.x - s0.x), static_cast<float>(su.y - s0.y));
                            const glm::vec2 dv(static_cast<float>(sv.x - s0.x), static_cast<float>(sv.y - s0.y));
                            const glm::vec2 md(static_cast<float>(mouse_coord.x - gizmo->asset_drag_start_mouse.x),
                                               static_cast<float>(mouse_coord.y - gizmo->asset_drag_start_mouse.y));

                            const float det = du.x * dv.y - du.y * dv.x;
                            if (std::fabs(det) > 1e-5f)
                            {
                                const float a = (md.x * dv.y - md.y * dv.x) / det;
                                const float b = (du.x * md.y - du.y * md.x) / det;
                                const glm::vec3 world_delta = u_world * (a * ref_len) + v_world * (b * ref_len);
                                gizmo->root_transform->SetLocalPosition(gizmo->asset_drag_start_position + world_delta);
                            }
                            else
                            {
                                const glm::vec3 world_delta = camera_right * (dx * kMoveSensitivity)
                                                            + camera_up * (-dy * kMoveSensitivity);
                                gizmo->root_transform->SetLocalPosition(gizmo->asset_drag_start_position + world_delta);
                            }
                        }
                    }
                    else
                    {
                        // Center/plane fallback: move in camera-aligned screen plane.
                        const glm::vec3 drag_right = (gizmo->asset_drag_mode == GizmoMode::MoveLocal)
                                                   ? glm::normalize(gizmo->asset_drag_start_rotation * math::AxisVector::X)
                                                   : camera_right;
                        const glm::vec3 drag_up = (gizmo->asset_drag_mode == GizmoMode::MoveLocal)
                                                ? glm::normalize(gizmo->asset_drag_start_rotation * math::AxisVector::Y)
                                                : camera_up;

                        const glm::vec3 world_delta = drag_right * (dx * kMoveSensitivity)
                                                    + drag_up * (-dy * kMoveSensitivity);
                        gizmo->root_transform->SetLocalPosition(gizmo->asset_drag_start_position + world_delta);
                    }
                }
                break;
            case GizmoMode::RotateWorld:
            case GizmoMode::RotateLocal:
                {
                    if (gizmo->asset_drag_pick_group >= 0)
                    {
                        const float delta_angle = compute_rotation_delta();
                        glm::vec3 axis = math::AxisVector::Z;

                        if (gizmo->asset_drag_pick_group < 3)
                        {
                            const bool local_space = (gizmo->asset_drag_mode == GizmoMode::RotateLocal);
                            axis = axis_from_index(gizmo->asset_drag_pick_group, local_space);
                        }
                        else if (camera_info)
                        {
                            axis = glm::normalize(math::Vector3f(camera_info->view[0][2],
                                                                 camera_info->view[1][2],
                                                                 camera_info->view[2][2]));
                        }

                        const glm::quat dq = glm::angleAxis(delta_angle, glm::normalize(axis));
                        gizmo->root_transform->SetLocalRotation(glm::normalize(dq * gizmo->asset_drag_start_rotation));
                    }
                    else
                    {
                        // Headless/no-pick fallback to previous deterministic drag behavior.
                        if (gizmo->asset_drag_mode == GizmoMode::RotateWorld)
                        {
                            const glm::quat yaw = glm::angleAxis(-dx * kRotateSensitivity, math::AxisVector::Y);
                            const glm::quat pitch = glm::angleAxis(-dy * kRotateSensitivity, camera_right);
                            gizmo->root_transform->SetLocalRotation(glm::normalize(yaw * pitch * gizmo->asset_drag_start_rotation));
                        }
                        else
                        {
                            const glm::vec3 local_x = gizmo->asset_drag_start_rotation * camera_right;
                            const glm::vec3 local_y = gizmo->asset_drag_start_rotation * camera_up;
                            const glm::quat yaw_local = glm::angleAxis(-dx * kRotateSensitivity, local_y);
                            const glm::quat pitch_local = glm::angleAxis(-dy * kRotateSensitivity, local_x);
                            gizmo->root_transform->SetLocalRotation(glm::normalize(yaw_local * pitch_local * gizmo->asset_drag_start_rotation));
                        }
                    }
                }
                break;
            case GizmoMode::ScaleLocal:
                {
                    glm::vec3 s = gizmo->asset_drag_start_scale;

                    if (gizmo->asset_drag_pick_group >= 0 && gizmo->asset_drag_pick_group < 3)
                    {
                        const glm::vec3 scale_axis = axis_from_index(gizmo->asset_drag_pick_group, true);
                        const float axis_pixels = project_mouse_delta_to_axis_pixels(scale_axis);
                        const float ratio = std::clamp(1.0f + axis_pixels * kScaleSensitivity, 0.05f, 10.0f);
                        s[gizmo->asset_drag_pick_group] *= ratio;
                    }
                    else if (gizmo->asset_drag_pick_shape == GizmoShape::Square && gizmo->asset_drag_pick_plane_normal_axis >= 0)
                    {
                        const float ratio = std::clamp(1.0f + (-dy) * kScaleSensitivity, 0.05f, 10.0f);
                        if (gizmo->asset_drag_pick_plane_normal_axis == 0)
                        {
                            s.y *= ratio;
                            s.z *= ratio;
                        }
                        else if (gizmo->asset_drag_pick_plane_normal_axis == 1)
                        {
                            s.x *= ratio;
                            s.z *= ratio;
                        }
                        else
                        {
                            s.x *= ratio;
                            s.y *= ratio;
                        }
                    }
                    else
                    {
                        const float ratio = std::clamp(1.0f + (-dy) * kScaleSensitivity, 0.05f, 10.0f);
                        s *= ratio;
                    }

                    NormalizeScaleByPolicy(s, gizmo->allow_negative_scale);
                    gizmo->root_transform->SetLocalScale(s);
                }
                break;
            }
        }

        if (gizmo->rotate_white_ring_transform && camera_info)
        {
            const math::Vector3f forward = glm::normalize(math::Vector3f(camera_info->view[0][2],
                                                                          camera_info->view[1][2],
                                                                          camera_info->view[2][2]));
            const math::Vector3f from = math::AxisVector::X;
            const float dot_value = glm::dot(from, forward);
            glm::quat facing(1.0f, 0.0f, 0.0f, 0.0f);

            if (dot_value < -0.9999f)
            {
                facing = glm::angleAxis(glm::radians(180.0f), math::AxisVector::Y);
            }
            else if (dot_value < 0.9999f)
            {
                const math::Vector3f axis = glm::normalize(glm::cross(from, forward));
                const float angle = std::acos(std::clamp(dot_value, -1.0f, 1.0f));
                facing = glm::angleAxis(angle, axis);
            }

            gizmo->rotate_white_ring_transform->SetLocalRotation(facing);
        }

        const math::Vector3f cur_pos = gizmo->root_transform->GetLocalPosition();
        const glm::quat cur_rot = gizmo->root_transform->GetLocalRotation();
        const math::Vector3f cur_scale = gizmo->root_transform->GetLocalScale();
        const bool changed = IsTransformChanged(prev_pos, prev_rot, prev_scale,
                                                cur_pos, cur_rot, cur_scale);

        if (gizmo->target_entity)
        {
            auto target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();
            if (target_transform && changed)
                target_transform->SetLocalTRS(cur_pos, cur_rot, cur_scale);
        }

        if (changed && gizmo->on_changed)
        {
            GizmoTransformChange change;
            change.previous_position = prev_pos;
            change.current_position = cur_pos;
            change.previous_rotation = prev_rot;
            change.current_rotation = cur_rot;
            change.previous_scale = prev_scale;
            change.current_scale = cur_scale;
            change.mode = gizmo->current_mode;
            gizmo->on_changed(change);
        }

        return;
    }

    if(gizmo->target_entity && gizmo->root_transform && !IsCurrentModeDragging(gizmo))
    {
        auto target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();
        if(target_transform)
        {
            gizmo->root_transform->SetLocalTRS(target_transform->GetLocalPosition(),
                                               target_transform->GetLocalRotation(),
                                               target_transform->GetLocalScale());
            SyncAllSubGizmoTransforms(gizmo);
        }
    }

    math::Vector3f prev_pos(0.0f);
    glm::quat prev_rot(1.0f, 0.0f, 0.0f, 0.0f);
    math::Vector3f prev_scale(1.0f);
    if(gizmo->root_transform)
    {
        prev_pos = gizmo->root_transform->GetLocalPosition();
        prev_rot = gizmo->root_transform->GetLocalRotation();
        prev_scale = gizmo->root_transform->GetLocalScale();
    }

    // Update only the active mode
    switch (gizmo->current_mode)
    {
    case GizmoMode::MoveWorld:
    case GizmoMode::MoveLocal:
    {
        UpdateMoveGizmoImpl((MoveGizmoImpl*)gizmo->move_impl, mouse_coord, camera_info, viewport_info, input_system, left_down, left_pressed, left_released);
        if(gizmo->root_transform)
        {
            math::Vector3f move_pos;
            if(GetMoveGizmoPosition((MoveGizmoImpl*)gizmo->move_impl, move_pos))
                gizmo->root_transform->SetLocalPosition(glm::vec3(move_pos));
        }
        break;
    }
    case GizmoMode::RotateWorld:
    case GizmoMode::RotateLocal:
    {
        UpdateRotateGizmoImpl((RotateGizmoImpl*)gizmo->rotate_impl, mouse_coord, camera_info, viewport_info, input_system, left_down, left_pressed, left_released);
        if(gizmo->root_transform)
        {
            RotateGizmoInteractionState state;
            if(GetRotateGizmoInteractionState((RotateGizmoImpl*)gizmo->rotate_impl, state))
            {
                if(state.dragging && state.pick_axis >= 0 && state.pick_axis <= 3)
                {
                    if(gizmo->last_rotate_axis != state.pick_axis)
                    {
                        gizmo->last_rotate_axis = state.pick_axis;
                        gizmo->last_rotate_angle = state.cur_angle;
                    }

                    const float delta = state.cur_angle - gizmo->last_rotate_angle;
                    if(std::fabs(delta) > 1e-6f)
                    {
                        math::Vector3f axis;
                        if(state.pick_axis < 3)
                        {
                            // Get base axis in world space
                            axis = math::GetAxisVector(math::AXIS(state.pick_axis));
                            
                            // For local rotation, transform axis to local space
                            if(gizmo->current_mode == GizmoMode::RotateLocal)
                            {
                                const glm::quat cur_rotation = gizmo->root_transform->GetLocalRotation();
                                axis = glm::vec3(cur_rotation * glm::vec4(axis, 0.0f));
                            }
                        }
                        else if(camera_info)
                        {
                            axis = glm::normalize(math::Vector3f(camera_info->view[0][2], camera_info->view[1][2], camera_info->view[2][2]));
                        }
                        else
                        {
                            axis = math::AxisVector::Z;
                        }

                        const glm::quat dq = glm::angleAxis(delta, glm::vec3(axis));
                        const glm::quat cur = gizmo->root_transform->GetLocalRotation();
                        gizmo->root_transform->SetLocalRotation(glm::normalize(dq * cur));
                    }

                    gizmo->last_rotate_angle = state.cur_angle;
                }
                else
                {
                    gizmo->last_rotate_axis = -1;
                    gizmo->last_rotate_angle = 0.0f;
                }
            }
        }
        break;
    }
    case GizmoMode::ScaleLocal:
    {
        UpdateScaleGizmoImpl((ScaleGizmoImpl*)gizmo->scale_impl, mouse_coord, camera_info, viewport_info, input_system, left_down, left_pressed, left_released);
        if(gizmo->root_transform)
        {
            ScaleGizmoInteractionState state;
            if(GetScaleGizmoInteractionState((ScaleGizmoImpl*)gizmo->scale_impl, state))
            {
                if(state.dragging && state.pick_axis >= 0)
                {
                    if(gizmo->last_scale_axis != state.pick_axis)
                    {
                        gizmo->last_scale_axis = state.pick_axis;
                        gizmo->last_scale_value = state.cur_scale;
                        gizmo->last_scale_value_u = state.cur_scale_u;
                        gizmo->last_scale_value_v = state.cur_scale_v;
                    }

                    float base = gizmo->last_scale_value;
                    if(std::fabs(base) < 1e-6f)
                        base = 1.0f;

                    const float ratio = state.cur_scale / base;
                    if(state.pick_axis < 3)
                    {
                        if(std::fabs(ratio - 1.0f) > 1e-6f)
                        {
                            glm::vec3 cur = gizmo->root_transform->GetLocalScale();
                            cur[state.pick_axis] *= ratio;
                            NormalizeScaleByPolicy(cur, gizmo->allow_negative_scale);
                            gizmo->root_transform->SetLocalScale(cur);
                        }
                    }
                    else if(state.pick_axis < 6)
                    {
                        float base_u = gizmo->last_scale_value_u;
                        float base_v = gizmo->last_scale_value_v;

                        if(std::fabs(base_u) < 1e-6f)
                            base_u = 1.0f;
                        if(std::fabs(base_v) < 1e-6f)
                            base_v = 1.0f;

                        const float ratio_u = state.cur_scale_u / base_u;
                        const float ratio_v = state.cur_scale_v / base_v;

                        if(std::fabs(ratio_u - 1.0f) > 1e-6f || std::fabs(ratio_v - 1.0f) > 1e-6f)
                        {
                            glm::vec3 cur = gizmo->root_transform->GetLocalScale();
                            static const int plane_axes[3][2] =
                            {
                                {1, 2}, // YZ
                                {0, 2}, // XZ
                                {0, 1}  // XY
                            };

                            const int plane_index = state.pick_axis - 3;
                            cur[plane_axes[plane_index][0]] *= ratio_u;
                            cur[plane_axes[plane_index][1]] *= ratio_v;

                            NormalizeScaleByPolicy(cur, gizmo->allow_negative_scale);
                            gizmo->root_transform->SetLocalScale(cur);
                        }
                    }

                    gizmo->last_scale_value = state.cur_scale;
                    gizmo->last_scale_value_u = state.cur_scale_u;
                    gizmo->last_scale_value_v = state.cur_scale_v;
                }
                else
                {
                    gizmo->last_scale_axis = -1;
                    gizmo->last_scale_value = 1.0f;
                    gizmo->last_scale_value_u = 1.0f;
                    gizmo->last_scale_value_v = 1.0f;
                }
            }
        }
        break;
    }
    }

    SyncAllSubGizmoTransforms(gizmo);

    if(gizmo->root_transform)
    {
        const math::Vector3f cur_pos = gizmo->root_transform->GetLocalPosition();
        const glm::quat cur_rot = gizmo->root_transform->GetLocalRotation();
        const math::Vector3f cur_scale = gizmo->root_transform->GetLocalScale();
        const bool changed = IsTransformChanged(prev_pos, prev_rot, prev_scale,
                                                cur_pos, cur_rot, cur_scale);

        if(gizmo->target_entity)
        {
            auto target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();
            if(target_transform)
            {
                if(IsTransformChanged(target_transform->GetLocalPosition(),
                                      target_transform->GetLocalRotation(),
                                      target_transform->GetLocalScale(),
                                      cur_pos,
                                      cur_rot,
                                      cur_scale))
                {
                    target_transform->SetLocalTRS(cur_pos, cur_rot, cur_scale);
                }
            }
        }

        if(changed && gizmo->on_changed)
        {
            GizmoTransformChange change;
            change.previous_position = prev_pos;
            change.current_position = cur_pos;
            change.previous_rotation = prev_rot;
            change.current_rotation = cur_rot;
            change.previous_scale = prev_scale;
            change.current_scale = cur_scale;
            change.mode = gizmo->current_mode;
            gizmo->on_changed(change);
        }
    }
}

bool EnsureGizmoSystemResources(hgl::ecs::ECSContext *world)
{
    if (g_gizmo_resident_state.resources_ready)
    {
        g_gizmo_resident_state.standby = false;
        return true;
    }

    if (!world)
        return false;

    auto *graphics = world->GetGraphicsContext();
    auto *render_context = world->GetRenderContext();
    auto *render_target = render_context ? render_context->GetCurrentRenderTarget() : nullptr;
    auto *render_pass = render_target ? render_target->GetRenderPass() : nullptr;

    if (!graphics || !render_pass)
        return false;

    if (!InitGizmoResource(graphics, render_pass))
        return false;

    g_gizmo_resident_state.resources_ready = true;
    g_gizmo_resident_state.standby = false;
    return true;
}

void ForceReleaseGizmoSystemResources()
{
    if (!g_gizmo_resident_state.resources_ready)
        return;

    FreeGizmoResource();
    g_gizmo_resident_state.resources_ready = false;
    g_gizmo_resident_state.standby = false;
}

bool IsGizmoSystemResourcesResident()
{
    return g_gizmo_resident_state.resources_ready;
}

glm::quat DirectionToRotation(const math::Vector3f &dir)
{
    const float len2 = glm::dot(dir, dir);
    const math::Vector3f forward = (len2 > 1e-8f) ? glm::normalize(dir) : math::AxisVector::Z;

    const math::Vector3f world_up = math::AxisVector::Y;
    math::Vector3f right = glm::cross(world_up, forward);
    if (glm::dot(right, right) < 1e-8f)
    {
        const math::Vector3f fallback_up = math::AxisVector::X;
        right = glm::cross(fallback_up, forward);
    }

    right = glm::normalize(right);
    const math::Vector3f up = glm::normalize(glm::cross(forward, right));

    glm::mat3 basis(1.0f);
    basis[0] = right;
    basis[1] = up;
    basis[2] = forward;
    return glm::normalize(glm::quat_cast(basis));
}

math::Vector3f RotationToDirection(const glm::quat &rot)
{
    const math::Vector3f forward = glm::normalize(rot * math::AxisVector::Z);
    return forward;
}

}//namespace hgl::graph




