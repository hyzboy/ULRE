#pragma once
#include"Gizmo.h"
#include<hgl/color/Color.h>

VK_NAMESPACE_BEGIN

class SceneNode;
class PrimitiveCreater;
class StaticMesh;

constexpr const COLOR gizmo_color[size_t(GizmoColor::RANGE_SIZE)]=
{
    COLOR::MozillaCharcoal,
    COLOR::BlanchedAlmond,

    COLOR::BlenderAxisRed,
    COLOR::BlenderAxisGreen,
    COLOR::BlenderAxisBlue,

    COLOR::BlenderYellow,
};

constexpr const float GIZMO_ARROW_LENGTH        =10.0f;                                                                 ///<箭头整体长度
constexpr const float GIZMO_CENTER_SPHERE_RADIUS=1.0f;                                                                  ///<中心球半径

constexpr const float GIZMO_CONE_LENGTH         =1.0f;                                                                  ///<圆锥高度
constexpr const float GIZMO_CONE_RADIUS         =1.0f;                                                                  ///<圆锥底部半径
constexpr const float GIZMO_CONE_OFFSET         =GIZMO_ARROW_LENGTH-(GIZMO_CONE_LENGTH/2.0f);                           ///<圆锥偏移量

constexpr const float GIZMO_CYLINDER_RADIUS     =0.25f;                                                                 ///<圆柱半径
constexpr const float GIZMO_CYLINDER_LENGTH     =GIZMO_ARROW_LENGTH-GIZMO_CENTER_SPHERE_RADIUS-GIZMO_CONE_LENGTH;       ///<圆柱长度

constexpr const float GIZMO_CYLINDER_HALF_LENGTH=GIZMO_CYLINDER_LENGTH/2.0f;                                            ///<圆柱一半长度
constexpr const float GIZMO_CYLINDER_OFFSET     =GIZMO_CYLINDER_HALF_LENGTH+GIZMO_CENTER_SPHERE_RADIUS;                 ///<圆柱偏移量

constexpr const float GIZMO_TWO_AXIS_OFFSET     =5.0F;                                                                  ///<二轴调节点偏移量(方片或圆)

Renderable *GetGizmoRenderable(const GizmoShape &gs,const GizmoColor &);

StaticMesh *CreateGizmoStaticMesh(SceneNode *);

VK_NAMESPACE_END
