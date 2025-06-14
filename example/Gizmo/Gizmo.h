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
    Square=0,   //方块
    Circle,     //圆
    Cube,       //立方体
    Sphere,     //球
    Cone,       //圆锥
    Cylinder,   //圆柱
    Torus,      //圆环

    ENUM_CLASS_RANGE(Square,Torus)
};

bool InitGizmoResource(RenderResource *);
void FreeGizmoResource();

Mesh *GetGizmoMesh(const GizmoShape &shape,const GizmoColor &color);

Mesh *GetGizmoMoveMesh();
Mesh *GetGizmoScaleMesh();
Mesh *GetGizmoRotateMesh();

VK_NAMESPACE_END
