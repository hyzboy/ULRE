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
#include"modes/GizmoModeRuntime.h"
#include"modes/MoveGizmoMode.h"
#include"modes/RotateGizmoMode.h"
#include"modes/ScaleGizmoMode.h"
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
#include<hgl/math/geometry/Ray.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/io/event/KeyboardEvent.h>
#include<glm/gtc/quaternion.hpp>
#include<glm/geometric.hpp>
#include<array>
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

    inline bool IsMoveMode(GizmoMode mode)   { return mode == GizmoMode::MoveWorld || mode == GizmoMode::MoveLocal; }
    inline bool IsRotateMode(GizmoMode mode) { return mode == GizmoMode::RotateWorld || mode == GizmoMode::RotateLocal; }
    inline bool IsScaleMode(GizmoMode mode)  { return mode == GizmoMode::ScaleLocal; }
    inline bool IsLocalMode(GizmoMode mode)  { return mode == GizmoMode::MoveLocal || mode == GizmoMode::RotateLocal || mode == GizmoMode::ScaleLocal; }
    inline bool IsWorldMode(GizmoMode mode)  { return !IsLocalMode(mode); }
}

// Global resident state definition - declared in GizmoInternal.h
GizmoSystemResidentState g_gizmo_resident_state;

struct GizmoECS
{
    // Type alias so existing code (AssetVisual.inl etc.) using GizmoECS::AssetVisualPrimitive compiles unchanged.
    using AssetVisualPrimitive = GizmoVisualPrimitive;

    hgl::ecs::ECSContext* world = nullptr;
    hgl::ecs::Entity* root = nullptr;
    std::shared_ptr<hgl::ecs::TransformComponent> root_transform;
    float fixed_pixel_diameter = GIZMO_FIXED_PIXEL_DIAMETER;

    std::vector<hgl::ecs::EntityID> asset_visual_entity_ids;
    uint32_t asset_mode_revision_counter = 1u;

    GizmoMode current_mode = GizmoMode::MoveWorld;
    bool allow_negative_scale = true;
    bool root_visible = true;

    hgl::ecs::Entity* target_entity = nullptr;
    GizmoChangedCallback on_changed;

    MoveGizmoMode   move_mode;
    RotateGizmoMode rotate_mode;
    ScaleGizmoMode  scale_mode;
};

// Asset backend initialization
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
void SetTransformGizmoFixedPixelDiameter(GizmoECS *gizmo, float pixel_diameter);
float GetTransformGizmoFixedPixelDiameter(const GizmoECS *gizmo);
void UpdateTransformGizmo(GizmoECS *gizmo,
                          const math::Vector2i &mouse_coord,
                          const CameraInfo *camera_info,
                          const ViewportInfo *viewport_info,
                          hgl::ecs::InputSystem *input_system,
                          bool left_down,
                          bool left_pressed,
                          bool left_released);

static void SyncAssetSubGizmoLocalTransforms(GizmoECS *gizmo);
static void SyncAssetFixedPixelSizingContext(GizmoECS *gizmo,
                                             const CameraInfo *camera_info,
                                             const ViewportInfo *viewport_info);
static void NormalizeScaleByPolicy(glm::vec3 &scale, bool allow_negative_scale);

static bool IsAnyModeDragging(const GizmoECS *gizmo)
{
    return gizmo && (gizmo->move_mode.IsDragging() || gizmo->rotate_mode.IsDragging() || gizmo->scale_mode.IsDragging());
}

#include "GizmoUnified.AssetCore.inl"
#include "GizmoUnified.AssetVisual.inl"
#include "modes/MoveGizmoMode.Visual.inl"
#include "modes/MoveGizmoMode.Input.inl"
#include "modes/RotateGizmoMode.Visual.inl"
#include "modes/RotateGizmoMode.Input.inl"
#include "modes/ScaleGizmoMode.Visual.inl"
#include "modes/ScaleGizmoMode.Input.inl"

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

    bool updated = false;

    if (gizmo->target_entity)
    {
        auto target_transform = gizmo->target_entity->GetComponent<hgl::ecs::TransformComponent>();
        if (target_transform)
        {
            glm::vec3 scale = target_transform->GetLocalScale();
            const glm::vec3 original_scale = scale;
            NormalizeScaleByPolicy(scale, gizmo->allow_negative_scale);

            if (glm::length(scale - original_scale) > 1e-6f)
            {
                target_transform->SetLocalScale(scale);
                updated = true;
            }
        }
    }
    else
    {
        glm::vec3 scale = gizmo->root_transform->GetLocalScale();
        const glm::vec3 original_scale = scale;
        NormalizeScaleByPolicy(scale, gizmo->allow_negative_scale);

        if (glm::length(scale - original_scale) > 1e-6f)
        {
            gizmo->root_transform->SetLocalScale(scale);
            updated = true;
        }
    }
}


GizmoECS *CreateTransformGizmo(hgl::ecs::ECSContext *world,
                               const char *name,
                               const math::Vector3f &position)
{
    if (!world)
        return nullptr;

    auto *gizmo = new GizmoECS;
    gizmo->world = world;
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
        gizmo->move_mode.entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Move");
        if (!gizmo->move_mode.entity)
        {
            std::cout << "[GizmoECS] Create move entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto move_transform = gizmo->move_mode.entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        move_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        move_transform->SetParent(gizmo->root->GetID());
        move_transform->SetFixedPixelSizingParameters(gizmo->fixed_pixel_diameter,
                                                      GIZMO_ARROW_LENGTH * 2.0f,
                                                      0.01f);
        move_transform->SetFixedPixelSizingEnabled(true);

        gizmo->move_mode.asset_instance = AttachGizmoAssetInstance(gizmo->move_mode.entity,
                                                               kGizmoMoveAssetWorldId,
                                                               ComposeGizmoInstanceId(gizmo->root->GetID(), 1u),
                                                               kGizmoMoveOverrideRef);
        gizmo->move_mode.BuildVisual(gizmo->world, gizmo->move_mode.entity,
                                     gizmo->asset_visual_entity_ids);
    }

    // Rotate Gizmo
    {
        gizmo->rotate_mode.entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Rotate");
        if (!gizmo->rotate_mode.entity)
        {
            std::cout << "[GizmoECS] Create rotate entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto rotate_transform = gizmo->rotate_mode.entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        rotate_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        rotate_transform->SetParent(gizmo->root->GetID());
        rotate_transform->SetFixedPixelSizingParameters(gizmo->fixed_pixel_diameter,
                                                        GIZMO_ARROW_LENGTH * 2.0f,
                                                        0.01f);
        rotate_transform->SetFixedPixelSizingEnabled(true);

        gizmo->rotate_mode.asset_instance = AttachGizmoAssetInstance(gizmo->rotate_mode.entity,
                                                                 kGizmoRotateAssetWorldId,
                                                                 ComposeGizmoInstanceId(gizmo->root->GetID(), 2u),
                                                                 kGizmoRotateOverrideRef);
        gizmo->rotate_mode.BuildVisual(gizmo->world, gizmo->rotate_mode.entity,
                                       gizmo->asset_visual_entity_ids);
    }

    // Scale Gizmo
    {
        gizmo->scale_mode.entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Scale");
        if (!gizmo->scale_mode.entity)
        {
            std::cout << "[GizmoECS] Create scale entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto scale_transform = gizmo->scale_mode.entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        scale_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        scale_transform->SetParent(gizmo->root->GetID());
        scale_transform->SetFixedPixelSizingParameters(gizmo->fixed_pixel_diameter,
                                                       GIZMO_ARROW_LENGTH * 2.0f,
                                                       0.01f);
        scale_transform->SetFixedPixelSizingEnabled(true);

        gizmo->scale_mode.asset_instance = AttachGizmoAssetInstance(gizmo->scale_mode.entity,
                                                                kGizmoScaleAssetWorldId,
                                                                ComposeGizmoInstanceId(gizmo->root->GetID(), 3u),
                                                                kGizmoScaleOverrideRef);
        gizmo->scale_mode.BuildVisual(gizmo->world, gizmo->scale_mode.entity,
                                       gizmo->asset_visual_entity_ids);
    }

    // Initialize with Move mode active
    SetTransformGizmoMode(gizmo, GizmoMode::MoveWorld);
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

    gizmo->move_mode.EndDrag();
    gizmo->rotate_mode.EndDrag();
    gizmo->scale_mode.EndDrag();

    if (gizmo->world)
    {
        for (const auto &id : gizmo->asset_visual_entity_ids)
        {
            if (id.IsValid())
                gizmo->world->DestroyEntity(id);
        }

        if (gizmo->move_mode.entity)
            gizmo->world->DestroyEntity(gizmo->move_mode.entity->GetID());
        if (gizmo->rotate_mode.entity)
            gizmo->world->DestroyEntity(gizmo->rotate_mode.entity->GetID());
        if (gizmo->scale_mode.entity)
            gizmo->world->DestroyEntity(gizmo->scale_mode.entity->GetID());
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

    SyncGizmoAssetModeBindings(gizmo);
    SyncAssetSubGizmoLocalTransforms(gizmo);
    gizmo->move_mode.EndDrag();
    gizmo->rotate_mode.EndDrag();
    gizmo->scale_mode.EndDrag();
    SetAssetVisualHighlight(gizmo, false);
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
                                       math::Vector3f(1.0f));

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

void SetTransformGizmoFixedPixelDiameter(GizmoECS *gizmo, float pixel_diameter)
{
    if (!gizmo)
        return;

    gizmo->fixed_pixel_diameter = SanitizeFixedPixelDiameter(pixel_diameter);
    ApplyAssetFixedPixelSizingParameters(gizmo);
}

float GetTransformGizmoFixedPixelDiameter(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->fixed_pixel_diameter : GIZMO_FIXED_PIXEL_DIAMETER;
}

bool IsTransformGizmoAllowNegativeScale(const GizmoECS *gizmo)
{
    return gizmo ? gizmo->allow_negative_scale : true;
}

#include "GizmoUnified.AssetUpdate.inl"

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






