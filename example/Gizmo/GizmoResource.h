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

Renderable *GetGizmoRenderable(const GizmoShape &gs,const GizmoColor &);

StaticMesh *CreateGizmoStaticMesh(SceneNode *);

VK_NAMESPACE_END
