#pragma once

#include <memory>
#include <vector>
#include "../GizmoInternal.h"  // GizmoShape

// Forward declarations for ECS types used in GizmoVisualPrimitive.
// Full types are required only at instantiation sites (GizmoUnified.cpp).
namespace hgl::ecs
{
class PrimitiveComponent;
class TransformComponent;
class AssetInstanceComponent;
class Entity;
} // namespace hgl::ecs

namespace hgl::graph
{

class MaterialInstance;

// Visual primitive entry owned by a GizmoMode object.
// (Was GizmoECS::AssetVisualPrimitive — extracted so Mode classes can own their own lists.)
struct GizmoVisualPrimitive
{
    std::shared_ptr<hgl::ecs::PrimitiveComponent>  primitive;
    std::shared_ptr<hgl::ecs::TransformComponent>  transform;
    MaterialInstance *base_material = nullptr;
    GizmoShape        shape         = GizmoShape::Sphere;
    int               group_id      = -1;
};

// Per-mode pick snapshot captured at drag-begin time.
// (Was GizmoECS::AssetDragState::ChannelState — extracted so Mode classes can own their own state.)
struct GizmoPickState
{
    int        pick_index            = -1;
    int        pick_group            = -1;
    int        pick_plane_normal_axis = -1;
    GizmoShape pick_shape            = GizmoShape::Sphere;
};

// Full drag state owned by MoveGizmoMode.
// Replaces the Move-specific fields that were scattered across GizmoECS::AssetDragState.
struct MoveDragState
{
    bool       active               = false;
    bool       mouse_captured       = false;
    hgl::ecs::InputSystem* capture_input_sys = nullptr;

    // Pick snapshot captured at drag-begin (equivalent to GizmoPickState).
    GizmoPickState pick;

    // Transform snapshot captured at drag-begin.
    hgl::math::Vector2i  start_mouse;
    hgl::math::Vector3f  start_position;
    glm::quat            start_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    hgl::math::Vector3f  start_scale{1.0f, 1.0f, 1.0f};

    // GizmoMode active at drag-begin (used by ApplyDrag to check IsLocalMode).
    GizmoMode mode = GizmoMode::MoveWorld;
};

// Full drag state owned by RotateGizmoMode.
struct RotateDragState
{
    bool       active               = false;
    bool       mouse_captured       = false;
    hgl::ecs::InputSystem* capture_input_sys = nullptr;

    // Pick snapshot captured at drag-begin.
    GizmoPickState pick;

    // Transform snapshot captured at drag-begin.
    hgl::math::Vector2i  start_mouse;
    hgl::math::Vector3f  start_position;
    glm::quat            start_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    hgl::math::Vector3f  start_scale{1.0f, 1.0f, 1.0f};

    // GizmoMode active at drag-begin (used by ApplyDrag to check IsLocalMode/IsWorldMode).
    GizmoMode mode = GizmoMode::RotateWorld;
};

} // namespace hgl::graph
