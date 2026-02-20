#pragma once

#include "Gizmo.h"

namespace hgl::graph{

struct GizmoMoveECS;
struct GizmoRotateECS;
struct GizmoScaleECS;

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
    float cur_scale_u = 1.0f;
    float cur_scale_v = 1.0f;
    float pick_scale_u = 1.0f;
    float pick_scale_v = 1.0f;
};

GizmoMoveECS *CreateGizmoMoveECS(::hgl::ecs::World *world,
                                 const char *name,
                                 const math::Vector3f &position);
void DestroyGizmoMoveECS(GizmoMoveECS *gizmo);
bool GetGizmoMoveECSState(const GizmoMoveECS *gizmo, GizmoMoveECSState &out_state);
bool GetGizmoMovePosition(const GizmoMoveECS *gizmo, math::Vector3f &out_position);
void SetGizmoMovePosition(GizmoMoveECS *gizmo, const math::Vector3f &position);
void SetGizmoMoveRotation(GizmoMoveECS *gizmo, const glm::quat &rotation);
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
void SetGizmoRotatePosition(GizmoRotateECS *gizmo, const math::Vector3f &position);
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
void SetGizmoScalePosition(GizmoScaleECS *gizmo, const math::Vector3f &position);
void SetGizmoScaleRotation(GizmoScaleECS *gizmo, const glm::quat &rotation);
void UpdateGizmoScaleECS(GizmoScaleECS *gizmo,
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

}//namespace hgl::graph