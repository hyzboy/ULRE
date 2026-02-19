#pragma once

#include<hgl/CoreType.h>
#include<hgl/vk/VK.h>
#include<hgl/math/VectorTypes.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/camera/ViewportInfo.h>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;
        class World;
        class Entity;
        class InputSystem;
    }
}

namespace hgl::graph{

struct GizmoMoveECS;
struct GizmoRotateECS;
struct GizmoScaleECS;

// 统一 Gizmo 世界（推荐使用）
class RenderPass;
struct GizmoECS;

enum class GizmoMode : int
{
    MoveWorld = 1,
    MoveLocal = 2,
    Rotate = 3,
    ScaleLocal = 4,

    Move = MoveWorld,
    Scale = ScaleLocal
};

struct GizmoMoveECSState
{
    int cur_axis = -1;
    int pick_axis = -1;
    bool dragging = false;
    float cur_dist = 0.0f;
    float pick_dist = 0.0f;
};

struct GizmoRotateECSState
{
    int cur_axis = -1;
    int pick_axis = -1;
    bool dragging = false;
    float cur_angle = 0.0f;
    float pick_angle = 0.0f;
};

struct GizmoScaleECSState
{
    int cur_axis = -1;
    int pick_axis = -1;
    bool dragging = false;
    float cur_scale = 1.0f;
    float pick_scale = 1.0f;
};

GizmoMoveECS *CreateGizmoMoveECS(::hgl::ecs::World *world,
                                 const char *name,
                                 const math::Vector3f &position);
void DestroyGizmoMoveECS(GizmoMoveECS *gizmo);
bool GetGizmoMoveECSState(const GizmoMoveECS *gizmo, GizmoMoveECSState &out_state);
void UpdateGizmoMoveECS(GizmoMoveECS *gizmo,
                        const math::Vector2i &mouse_coord,
                        const CameraInfo *camera_info,
                        const ViewportInfo *viewport_info,
                        ::hgl::ecs::InputSystem *input_system,
                        bool left_down,
                        bool left_pressed,
                        bool left_released);

GizmoRotateECS *CreateGizmoRotateECS(::hgl::ecs::World *world,
                                      const char *name,
                                      const math::Vector3f &position);
void DestroyGizmoRotateECS(GizmoRotateECS *gizmo);
bool GetGizmoRotateECSState(const GizmoRotateECS *gizmo, GizmoRotateECSState &out_state);
void UpdateGizmoRotateECS(GizmoRotateECS *gizmo,
                          const math::Vector2i &mouse_coord,
                          const CameraInfo *camera_info,
                          const ViewportInfo *viewport_info,
                          ::hgl::ecs::InputSystem *input_system,
                          bool left_down,
                          bool left_pressed,
                          bool left_released);

GizmoScaleECS *CreateGizmoScaleECS(::hgl::ecs::World *world,
                                    const char *name,
                                    const math::Vector3f &position);
void DestroyGizmoScaleECS(GizmoScaleECS *gizmo);
bool GetGizmoScaleECSState(const GizmoScaleECS *gizmo, GizmoScaleECSState &out_state);
void UpdateGizmoScaleECS(GizmoScaleECS *gizmo,
                         const math::Vector2i &mouse_coord,
                         const CameraInfo *camera_info,
                         const ViewportInfo *viewport_info,
                         ::hgl::ecs::InputSystem *input_system,
                         bool left_down,
                         bool left_pressed,
                         bool left_released);

/// ============= Unified Gizmo Interface (Recommended) =============

GizmoECS *CreateGizmoECS(::hgl::ecs::ECSContext *world,
                         const char *name,
                         const math::Vector3f &position);
void DestroyGizmoECS(GizmoECS *gizmo);

void SetGizmoMode(GizmoECS *gizmo, GizmoMode mode);
GizmoMode GetGizmoMode(const GizmoECS *gizmo);

void SetGizmoVisible(GizmoECS *gizmo, bool visible);
hgl::ecs::Entity *GetGizmoRootEntity(const GizmoECS *gizmo);

void UpdateGizmoECS(GizmoECS *gizmo,
                    const math::Vector2i &mouse_coord,
                    const CameraInfo *camera_info,
                    const ViewportInfo *viewport_info,
                    ::hgl::ecs::InputSystem *input_system,
                    bool left_down,
                    bool left_pressed,
                    bool left_released);

void SetGizmoMoveVisible(GizmoMoveECS *gizmo, bool visible);
void SetGizmoRotateVisible(GizmoRotateECS *gizmo, bool visible);
void SetGizmoScaleVisible(GizmoScaleECS *gizmo, bool visible);

enum class GizmoColor:uint
{
    Black=0,
    White,

    Red,
    Green,
    Blue,

    Yellow,

    ENUM_CLASS_RANGE(Black,Yellow)
};

enum class GizmoShape:uint
{
    Square=0,   //方块
    Circle,     //圆
    Cube,       //立方体
    Sphere,     //球
    Cone,       //圆锥
    Cylinder,   //圆柱
    Torus,      //圆环

    ENUM_CLASS_RANGE(Square,Torus)
};

bool InitGizmoResource(GraphicsContext *, RenderPass *);
void FreeGizmoResource();

MaterialInstance *GetGizmoMI3D(const GizmoColor &);
Primitive *GetGizmoMeshPrimitive(const GizmoShape &shape);

}//namespace hgl::graph
