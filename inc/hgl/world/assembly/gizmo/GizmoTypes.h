#pragma once

#include<hgl/math/VectorTypes.h>
#include<glm/gtc/quaternion.hpp>
#include<functional>

namespace hgl::graph
{
    // 统一 Gizmo 模式枚举
    enum class GizmoMode : int
    {
        MoveWorld = 1,
        MoveLocal = 2,
        RotateWorld = 3,
        RotateLocal = 4,
        ScaleLocal = 5,

        Move = MoveWorld,
        Rotate = RotateWorld,
        Scale = ScaleLocal
    };

    // Gizmo 变换变化回调参数
    struct GizmoTransformChange
    {
        math::Vector3f previous_position;
        math::Vector3f current_position;
        glm::quat previous_rotation;
        glm::quat current_rotation;
        math::Vector3f previous_scale;
        math::Vector3f current_scale;
        GizmoMode mode = GizmoMode::MoveWorld;
    };

    // Gizmo 变换变化回调函数类型
    using GizmoChangedCallback = std::function<void(const GizmoTransformChange&)>;

    // Gizmo ECS 内部实现结构（不透明类型）
    struct GizmoECS;
}//namespace hgl::graph
