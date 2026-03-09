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

#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/VisibilityComponent.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
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
    hgl::ecs::ECSContext* world = nullptr;
    hgl::ecs::Entity* root = nullptr;
    std::shared_ptr<hgl::ecs::TransformComponent> root_transform;
    float fixed_pixel_diameter = GIZMO_FIXED_PIXEL_DIAMETER;

    std::vector<hgl::ecs::EntityID> asset_visual_entity_ids;

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
void UpdateTransformGizmo(GizmoECS *gizmo, const GizmoFrameInput &input);

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

static bool IsTransformChanged(const GizmoPrevTransform &prev,
                               const GizmoPrevTransform &cur)
{
    if (!IsNearlyEqual(prev.pos, cur.pos))
        return true;

    if (!IsNearlyEqualRotation(prev.rot, cur.rot))
        return true;

    if (!IsNearlyEqual(prev.scale, cur.scale))
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

math::Vector3f RotationToDirection(const glm::quat &rot)
{
    const math::Vector3f forward = glm::normalize(rot * math::AxisVector::Z);
    return forward;
}

}//namespace hgl::graph






