#pragma once

#include"GizmoTypes.h"
#include<hgl/vk/VK.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/camera/ViewportInfo.h>
#include<hgl/ecs/core/System.h>
#include<memory>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;
        class World;
        class Entity;
        class InputSystem;
        class CameraSystem;
        class TransformComponent;
        class EnvironmentSystem;
    }
}

namespace hgl::graph{

// 统一 Gizmo 世界（推荐使用）
class RenderPass;

}//namespace hgl::graph

/// ============= Unified Gizmo Interface (Recommended for external users) =============
/// 外部开发者建议只使用以下 Unified/System API。
/// Move/Rotate/Scale 与资源层细节已转入内部头（GizmoInternal.h）。

#include"TransformGizmoSystem.h"
#include"SunDirectionControlSystem.h"
