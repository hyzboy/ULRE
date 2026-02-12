#pragma once

#include<hgl/graph/VK.h>
#include<hgl/component/Component.h>
#include<hgl/math/VectorTypes.h>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;
    }
}

VK_NAMESPACE_BEGIN

struct CameraInfo;
class ViewportInfo;

struct GizmoMoveECS;

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

bool InitGizmoResource(RenderFramework *);
void FreeGizmoResource();

MaterialInstance *GetGizmoMI3D(const GizmoColor &);
COMPONENT_NAMESPACE::ComponentDataPtr GetGizmoMeshCDP(const GizmoShape &shape);

GizmoMoveECS *CreateGizmoMoveECS(::hgl::ecs::ECSContext *world,
                                 const char *name,
                                 const math::Vector3f &position);
void DestroyGizmoMoveECS(GizmoMoveECS *gizmo);
void UpdateGizmoMoveECS(GizmoMoveECS *gizmo,
                        const math::Vector2i &mouse_coord,
                        const CameraInfo *camera_info,
                        const ViewportInfo *viewport_info,
                        bool left_down,
                        bool left_pressed,
                        bool left_released);

VK_NAMESPACE_END
