#pragma once

#include "Gizmo.h"

namespace hgl::graph{

using TransformGizmo = GizmoECS;

TransformGizmo *CreateTransformGizmo(::hgl::ecs::ECSContext *world,
                                     const char *name,
                                     const math::Vector3f &position);
TransformGizmo *CreateDefaultTransformGizmo(::hgl::ecs::ECSContext *world,
                                            const char *name,
                                            const math::Vector3f &position,
                                            GizmoMode default_mode = GizmoMode::MoveWorld);
void DestroyTransformGizmo(TransformGizmo *gizmo);

void SetTransformGizmoMode(TransformGizmo *gizmo, GizmoMode mode);
GizmoMode GetTransformGizmoMode(const TransformGizmo *gizmo);

void SetTransformGizmoVisible(TransformGizmo *gizmo, bool visible);
bool BindTransformGizmoTargetEntity(TransformGizmo *gizmo, hgl::ecs::Entity *target_entity);
hgl::ecs::Entity *GetTransformGizmoTargetEntity(const TransformGizmo *gizmo);
void SetTransformGizmoChangedCallback(TransformGizmo *gizmo, GizmoChangedCallback callback);
void SetTransformGizmoAllowNegativeScale(TransformGizmo *gizmo, bool enabled);
bool IsTransformGizmoAllowNegativeScale(const TransformGizmo *gizmo);
void UpdateTransformGizmo(TransformGizmo *gizmo,
                          const math::Vector2i &mouse_coord,
                          const CameraInfo *camera_info,
                          const ViewportInfo *viewport_info,
                          ::hgl::ecs::InputSystem *input_system,
                          bool left_down,
                          bool left_pressed,
                          bool left_released);

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
    Square=0,
    Circle,
    Cube,
    Sphere,
    Cone,
    Cylinder,
    Torus,

    ENUM_CLASS_RANGE(Square,Torus)
};

struct MoveGizmoImpl;
struct RotateGizmoImpl;
struct ScaleGizmoImpl;

struct MoveGizmoInteractionState
{
    int cur_axis = -1;
    int pick_axis = -1;
    bool dragging = false;
    float cur_dist = 0.0f;
    float pick_dist = 0.0f;
};

struct RotateGizmoInteractionState
{
    int cur_axis = -1;
    int pick_axis = -1;
    bool dragging = false;
    float cur_angle = 0.0f;
    float pick_angle = 0.0f;
};

struct ScaleGizmoInteractionState
{
    int cur_axis = -1;
    int pick_axis = -1;
    bool dragging = false;
    float cur_scale = 1.0f;
    float pick_scale = 1.0f;
    float cur_scale_u = 1.0f;
    float cur_scale_v = 1.0f;
    float pick_scale_u = 1.0f;
    float pick_scale_v = 1.0f;
};

MoveGizmoImpl *CreateMoveGizmoImpl(::hgl::ecs::World *world,
                                   const char *name,
                                   const math::Vector3f &position);
void DestroyMoveGizmoImpl(MoveGizmoImpl *gizmo);
bool GetMoveGizmoInteractionState(const MoveGizmoImpl *gizmo, MoveGizmoInteractionState &out_state);
bool GetMoveGizmoPosition(const MoveGizmoImpl *gizmo, math::Vector3f &out_position);
void SetMoveGizmoPosition(MoveGizmoImpl *gizmo, const math::Vector3f &position);
void SetMoveGizmoRotation(MoveGizmoImpl *gizmo, const glm::quat &rotation);
void UpdateMoveGizmoImpl(MoveGizmoImpl *gizmo,
                         const math::Vector2i &mouse_coord,
                         const CameraInfo *camera_info,
                         const ViewportInfo *viewport_info,
                         ::hgl::ecs::InputSystem *input_system,
                         bool left_down,
                         bool left_pressed,
                         bool left_released);

RotateGizmoImpl *CreateRotateGizmoImpl(::hgl::ecs::World *world,
                                       const char *name,
                                       const math::Vector3f &position);
void DestroyRotateGizmoImpl(RotateGizmoImpl *gizmo);
bool GetRotateGizmoInteractionState(const RotateGizmoImpl *gizmo, RotateGizmoInteractionState &out_state);
void SetRotateGizmoPosition(RotateGizmoImpl *gizmo, const math::Vector3f &position);
void SetRotateGizmoRotation(RotateGizmoImpl *gizmo, const glm::quat &rotation);
void UpdateRotateGizmoImpl(RotateGizmoImpl *gizmo,
                           const math::Vector2i &mouse_coord,
                           const CameraInfo *camera_info,
                           const ViewportInfo *viewport_info,
                           ::hgl::ecs::InputSystem *input_system,
                           bool left_down,
                           bool left_pressed,
                           bool left_released);

ScaleGizmoImpl *CreateScaleGizmoImpl(::hgl::ecs::World *world,
                                     const char *name,
                                     const math::Vector3f &position);
void DestroyScaleGizmoImpl(ScaleGizmoImpl *gizmo);
bool GetScaleGizmoInteractionState(const ScaleGizmoImpl *gizmo, ScaleGizmoInteractionState &out_state);
void SetScaleGizmoPosition(ScaleGizmoImpl *gizmo, const math::Vector3f &position);
void SetScaleGizmoRotation(ScaleGizmoImpl *gizmo, const glm::quat &rotation);
void UpdateScaleGizmoImpl(ScaleGizmoImpl *gizmo,
                          const math::Vector2i &mouse_coord,
                          const CameraInfo *camera_info,
                          const ViewportInfo *viewport_info,
                          ::hgl::ecs::InputSystem *input_system,
                          bool left_down,
                          bool left_pressed,
                          bool left_released);

void SetMoveGizmoVisible(MoveGizmoImpl *gizmo, bool visible);
void SetRotateGizmoVisible(RotateGizmoImpl *gizmo, bool visible);
void SetScaleGizmoVisible(ScaleGizmoImpl *gizmo, bool visible);

bool InitGizmoResource(GraphicsContext *, RenderPass *);
void FreeGizmoResource();
bool EnsureGizmoSystemResources(::hgl::ecs::ECSContext *world);
void ForceReleaseGizmoSystemResources();
bool IsGizmoSystemResourcesResident();

MaterialInstance *GetGizmoMI3D(const GizmoColor &);
Primitive *GetGizmoMeshPrimitive(const GizmoShape &shape);

}//namespace hgl::graph