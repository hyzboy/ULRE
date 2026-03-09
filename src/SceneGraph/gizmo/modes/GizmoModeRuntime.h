#pragma once

#include <memory>
#include <vector>
#include "../GizmoInternal.h"  // 引入 GizmoShape

// 用于 `GizmoVisualPrimitive` 的 ECS 类型的前向声明。
// 完整类型仅在实例化位置需要（`GizmoUnified.cpp`）。
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

    // 由 `GizmoMode` 对象拥有的可视原语条目。
    // （原为 `GizmoECS::AssetVisualPrimitive` — 已提取以便各 Mode 类可以拥有自己的列表。）
    struct GizmoVisualPrimitive
    {
        std::shared_ptr<hgl::ecs::PrimitiveComponent>  primitive;
        std::shared_ptr<hgl::ecs::TransformComponent>  transform;
        MaterialInstance *base_material = nullptr;
        GizmoShape        shape         = GizmoShape::Sphere;
        int               group_id      = -1;
    };

    // 每个模式在拖拽开始时捕获的拾取快照。
    // （原为 `GizmoECS::AssetDragState::ChannelState` — 已提取以便各 Mode 类可以拥有自己的状态。）
    struct GizmoPickState
    {
        int        pick_index            = -1;
        int        pick_group            = -1;
        int        pick_plane_normal_axis = -1;
        GizmoShape pick_shape            = GizmoShape::Sphere;
    };

    // 由 `MoveGizmoMode` 拥有的完整拖拽状态。
    // 替代分散在 `GizmoECS::AssetDragState` 中的 Move 特定字段。
    struct MoveDragState
    {
        bool       active               = false;
        bool       mouse_captured       = false;
        hgl::ecs::InputSystem* capture_input_sys = nullptr;

        // 在拖拽开始时捕获的拾取快照（等同于 `GizmoPickState`）。
        GizmoPickState pick;

        // 在拖拽开始时捕获的变换快照。
        hgl::math::Vector2i  start_mouse;
        hgl::math::Vector3f  start_position;
        glm::quat            start_rotation{1.0f, 0.0f, 0.0f, 0.0f};
        hgl::math::Vector3f  start_scale{1.0f, 1.0f, 1.0f};

        // 拖拽开始时的 `GizmoMode`（由 `ApplyDrag` 用于检查是否为本地模式）。
        GizmoMode mode = GizmoMode::MoveWorld;
    };

    // 由 `RotateGizmoMode` 拥有的完整拖拽状态。
    struct RotateDragState
    {
        bool       active               = false;
        bool       mouse_captured       = false;
        hgl::ecs::InputSystem* capture_input_sys = nullptr;

        // 在拖拽开始时捕获的拾取快照。
        GizmoPickState pick;

        // 在拖拽开始时捕获的变换快照。
        hgl::math::Vector2i  start_mouse;
        hgl::math::Vector3f  start_position;
        glm::quat            start_rotation{1.0f, 0.0f, 0.0f, 0.0f};
        hgl::math::Vector3f  start_scale{1.0f, 1.0f, 1.0f};

        // 拖拽开始时的 `GizmoMode`（由 `ApplyDrag` 用于检查是本地模式还是世界模式）。
        GizmoMode mode = GizmoMode::RotateWorld;
    };

    // 由 `ScaleGizmoMode` 拥有的完整拖拽状态。
    struct ScaleDragState
    {
        bool       active               = false;
        bool       mouse_captured       = false;
        hgl::ecs::InputSystem* capture_input_sys = nullptr;

        // 在拖拽开始时捕获的拾取快照。
        GizmoPickState pick;

        // 在拖拽开始时捕获的变换快照。
        hgl::math::Vector2i  start_mouse;
        hgl::math::Vector3f  start_position;
        glm::quat            start_rotation{1.0f, 0.0f, 0.0f, 0.0f};
        hgl::math::Vector3f  start_scale{1.0f, 1.0f, 1.0f};

        // 拖拽开始时的 `GizmoMode`（对缩放始终为 `ScaleLocal`）。
        GizmoMode mode = GizmoMode::ScaleLocal;
    };
} // namespace hgl::graph
