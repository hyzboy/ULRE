#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

class Gizmo
{
protected:

    Material *          material;
    MaterialInstance *  mtl_inst;
    Pipeline *          pipeline;
    VertexDataManager * vdm;

    Primitive *         prim_cylinder;      ///<圆柱
    Primitive *         prim_cone;          ///<圆锥
    Primitive *         prim_sphere;        ///<球体
    Primitive *         prim_cube;          ///<立方体
    Primitive *         prim_plane;         ///<平面

    
};//class Gizmo

VK_NAMESPACE_END
