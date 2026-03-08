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
#include"GizmoController.h"
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

    // Rotate and Scale keep the old ChannelRuntime layout; Move data now lives in move_mode.
    struct ChannelRuntime
    {
        hgl::ecs::Entity *entity = nullptr;
        std::shared_ptr<hgl::ecs::AssetInstanceComponent> asset_instance;
        std::vector<AssetVisualPrimitive> primitives;
        // Optional transform handle (only used by RotateChannel for the view ring).
        std::shared_ptr<hgl::ecs::TransformComponent> aux_transform;
    };

    ChannelRuntime rotate_channel;
    ChannelRuntime scale_channel;

    std::vector<hgl::ecs::EntityID> asset_visual_entity_ids;
    uint32_t asset_mode_revision_counter = 1u;
    bool asset_visual_highlighted = false;
    int asset_hovered_visual_index = -1;

    // Move data now lives in move_mode; provide same accessor name so call sites are unchanged.
    MoveGizmoMode   &MoveChannel()       { return move_mode; }
    const MoveGizmoMode &MoveChannel() const { return move_mode; }
    ChannelRuntime  &RotateChannel()       { return rotate_channel; }
    const ChannelRuntime &RotateChannel() const { return rotate_channel; }
    ChannelRuntime  &ScaleChannel()        { return scale_channel; }
    const ChannelRuntime &ScaleChannel() const { return scale_channel; }

    // Asset backend interaction state, split by logical channel.
    struct AssetDragState
    {
        // Type alias so GetAssetChannelState can return GizmoPickState& for all modes.
        using ChannelState = GizmoPickState;

        bool dragging = false;
        GizmoMode mode = GizmoMode::MoveWorld;
        bool mouse_captured = false;
        hgl::ecs::InputSystem *capture_input_system = nullptr;
        math::Vector2i start_mouse{0, 0};
        math::Vector3f start_position{0.0f, 0.0f, 0.0f};
        glm::quat start_rotation{1.0f, 0.0f, 0.0f, 0.0f};
        math::Vector3f start_scale{1.0f, 1.0f, 1.0f};

        // Active pick snapshot (used by existing flow).
        int pick_index = -1;
        int pick_group = -1;
        int pick_plane_normal_axis = -1;
        GizmoShape pick_shape = GizmoShape::Sphere;

        // Per-channel pick snapshots: Move's is now in GizmoECS::move_mode.pick_state.
        ChannelState rotate;
        ChannelState scale;
    };

    AssetDragState asset_drag;

    GizmoMode current_mode = GizmoMode::MoveWorld;
    bool allow_negative_scale = true;
    bool root_visible = true;

    hgl::ecs::Entity* target_entity = nullptr;
    GizmoChangedCallback on_changed;
    GizmoController channel_controller;

    // Mode objects: Move owns its visual+pick data; Rotate/Scale skeletons for Phase 4.
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
static void ApplyAssetMoveDragChannel(GizmoECS *gizmo,
                                      const math::Vector2i &mouse_coord,
                                      const CameraInfo *camera_info,
                                      const ViewportInfo *viewport_info,
                                      const glm::vec3 &camera_right,
                                      const glm::vec3 &camera_up,
                                      float dx,
                                      float dy,
                                      float move_sensitivity);
static void ApplyAssetRotateDragChannel(GizmoECS *gizmo,
                                        const math::Vector2i &mouse_coord,
                                        const CameraInfo *camera_info,
                                        const ViewportInfo *viewport_info,
                                        const glm::vec3 &camera_right,
                                        const glm::vec3 &camera_up,
                                        float dx,
                                        float dy,
                                        float rotate_sensitivity);
static void ApplyAssetScaleDragChannel(GizmoECS *gizmo,
                                       const math::Vector2i &mouse_coord,
                                       const CameraInfo *camera_info,
                                       const ViewportInfo *viewport_info,
                                       float dy,
                                       float scale_sensitivity,
                                       const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                                       bool has_view_context,
                                       math::Vector3f &cur_effective_scale);

static void DispatchActiveAssetDragChannel(GizmoECS *gizmo,
                                           const math::Vector2i &mouse_coord,
                                           const CameraInfo *camera_info,
                                           const ViewportInfo *viewport_info,
                                           const std::shared_ptr<hgl::ecs::TransformComponent> &target_transform,
                                           bool has_view_context,
                                           math::Vector3f &cur_effective_scale)
{
    const float dx = static_cast<float>(mouse_coord.x - gizmo->asset_drag.start_mouse.x);
    const float dy = static_cast<float>(mouse_coord.y - gizmo->asset_drag.start_mouse.y);

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

    switch (GizmoController::SlotForMode(gizmo->asset_drag.mode))
    {
    case GizmoController::ChannelSlot::Move:
        ApplyAssetMoveDragChannel(gizmo,
                                  mouse_coord,
                                  camera_info,
                                  viewport_info,
                                  camera_right,
                                  camera_up,
                                  dx,
                                  dy,
                                  kMoveSensitivity);
        break;
    case GizmoController::ChannelSlot::Rotate:
        ApplyAssetRotateDragChannel(gizmo,
                                    mouse_coord,
                                    camera_info,
                                    viewport_info,
                                    camera_right,
                                    camera_up,
                                    dx,
                                    dy,
                                    kRotateSensitivity);
        break;
    case GizmoController::ChannelSlot::Scale:
        ApplyAssetScaleDragChannel(gizmo,
                                   mouse_coord,
                                   camera_info,
                                   viewport_info,
                                   dy,
                                   kScaleSensitivity,
                                   target_transform,
                                   has_view_context,
                                   cur_effective_scale);
        break;
    }
}

static GizmoECS::ChannelRuntime &GetActiveChannelRuntime(GizmoECS *gizmo)
{
    // Move data now lives in move_mode; only Rotate/Scale use ChannelRuntime.
    if (GizmoController::IsRotateMode(gizmo->current_mode))
        return gizmo->rotate_channel;
    return gizmo->scale_channel;
}

static const GizmoECS::ChannelRuntime &GetActiveChannelRuntime(const GizmoECS *gizmo)
{
    if (GizmoController::IsRotateMode(gizmo->current_mode))
        return gizmo->rotate_channel;
    return gizmo->scale_channel;
}

#include "GizmoUnified.AssetCore.inl"
#include "GizmoUnified.AssetVisual.inl"
#include "GizmoUnified.AssetChannels.inl"
#include "channels/MoveGizmoChannel.Runtime.inl"

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
    gizmo->channel_controller.InitializeDefaultChannels();
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
        gizmo->MoveChannel().entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Move");
        if (!gizmo->MoveChannel().entity)
        {
            std::cout << "[GizmoECS] Create move entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto move_transform = gizmo->MoveChannel().entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        move_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        move_transform->SetParent(gizmo->root->GetID());
        move_transform->SetFixedPixelSizingParameters(gizmo->fixed_pixel_diameter,
                                                      GIZMO_ARROW_LENGTH * 2.0f,
                                                      0.01f);
        move_transform->SetFixedPixelSizingEnabled(true);

        gizmo->MoveChannel().asset_instance = AttachGizmoAssetInstance(gizmo->MoveChannel().entity,
                                                               kGizmoMoveAssetWorldId,
                                                               ComposeGizmoInstanceId(gizmo->root->GetID(), 1u),
                                                               kGizmoMoveOverrideRef);
        BuildMoveAssetVisual(gizmo, gizmo->MoveChannel().entity);
    }

    // Rotate Gizmo
    {
        gizmo->RotateChannel().entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Rotate");
        if (!gizmo->RotateChannel().entity)
        {
            std::cout << "[GizmoECS] Create rotate entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto rotate_transform = gizmo->RotateChannel().entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        rotate_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        rotate_transform->SetParent(gizmo->root->GetID());
        rotate_transform->SetFixedPixelSizingParameters(gizmo->fixed_pixel_diameter,
                                                        GIZMO_ARROW_LENGTH * 2.0f,
                                                        0.01f);
        rotate_transform->SetFixedPixelSizingEnabled(true);

        gizmo->RotateChannel().asset_instance = AttachGizmoAssetInstance(gizmo->RotateChannel().entity,
                                                                 kGizmoRotateAssetWorldId,
                                                                 ComposeGizmoInstanceId(gizmo->root->GetID(), 2u),
                                                                 kGizmoRotateOverrideRef);
        BuildRotateAssetVisual(gizmo, gizmo->RotateChannel().entity);
    }

    // Scale Gizmo
    {
        gizmo->ScaleChannel().entity = world->CreateEntity<hgl::ecs::Entity>("Gizmo_Scale");
        if (!gizmo->ScaleChannel().entity)
        {
            std::cout << "[GizmoECS] Create scale entity failed" << std::endl;
            DestroyTransformGizmo(gizmo);
            return nullptr;
        }

        auto scale_transform = gizmo->ScaleChannel().entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        scale_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        scale_transform->SetParent(gizmo->root->GetID());
        scale_transform->SetFixedPixelSizingParameters(gizmo->fixed_pixel_diameter,
                                                       GIZMO_ARROW_LENGTH * 2.0f,
                                                       0.01f);
        scale_transform->SetFixedPixelSizingEnabled(true);

        gizmo->ScaleChannel().asset_instance = AttachGizmoAssetInstance(gizmo->ScaleChannel().entity,
                                                                kGizmoScaleAssetWorldId,
                                                                ComposeGizmoInstanceId(gizmo->root->GetID(), 3u),
                                                                kGizmoScaleOverrideRef);
        BuildScaleAssetVisual(gizmo, gizmo->ScaleChannel().entity);
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

    EndAssetMouseCapture(gizmo);

    if (gizmo->world)
    {
        for (const auto &id : gizmo->asset_visual_entity_ids)
        {
            if (id.IsValid())
                gizmo->world->DestroyEntity(id);
        }

        if (gizmo->MoveChannel().entity)
            gizmo->world->DestroyEntity(gizmo->MoveChannel().entity->GetID());
        if (gizmo->RotateChannel().entity)
            gizmo->world->DestroyEntity(gizmo->RotateChannel().entity->GetID());
        if (gizmo->ScaleChannel().entity)
            gizmo->world->DestroyEntity(gizmo->ScaleChannel().entity->GetID());
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

    // Phase-in OOP hook: notify active channel on mode activation.
    if (auto *channel = gizmo->channel_controller.GetChannelForMode(mode))
        channel->OnModeActivated(gizmo, mode);

    gizmo->current_mode = mode;
    std::cout << "[GizmoECS] Set mode=" << static_cast<int>(mode) << std::endl;

    SyncGizmoAssetModeBindings(gizmo);
    SyncAssetSubGizmoLocalTransforms(gizmo);
    EndAssetMouseCapture(gizmo);
    gizmo->asset_drag.dragging = false;
    ResetAssetActivePickState(gizmo);
    SetAssetVisualHighlight(gizmo, false);
    gizmo->asset_hovered_visual_index = -1;
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






