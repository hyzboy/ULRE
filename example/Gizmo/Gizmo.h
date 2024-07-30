#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

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
    Plane=0,    //平面
    Cube,       //立方体
    Sphere,     //球
    Cone,       //圆锥
    Cylinder,   //圆柱
    Torus,      //圆环

    ENUM_CLASS_RANGE(Plane,Torus)
};

bool InitGizmoResource(RenderResource *);
void FreeGizmoResource();

StaticMesh *GetGizmoMoveStaticMesh();
StaticMesh *GetGizmoScaleStaticMesh();
StaticMesh *GetGizmoRotateStaticMesh();

VK_NAMESPACE_END
