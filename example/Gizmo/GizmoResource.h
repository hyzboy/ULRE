#pragma once
#include"Gizmo.h"

VK_NAMESPACE_BEGIN
constexpr const COLOR gizmo_color[size_t(GizmoColor::RANGE_SIZE)]=
{
    COLOR::MozillaCharcoal,
    COLOR::BlanchedAlmond,

    COLOR::BlenderAxisRed,
    COLOR::BlenderAxisGreen,
    COLOR::BlenderAxisBlue,

    COLOR::BlenderYellow,
};

static RenderResource *  gizmo_rr=nullptr;

struct GizmoResource
{
    Material *           mtl;
    MaterialInstance *   mi[size_t(GizmoColor::RANGE_SIZE)];
    Pipeline *           pipeline;
    VertexDataManager *  vdm;

    PrimitiveCreater *   prim_creater;
};

static GizmoResource    gizmo_line{};
static GizmoResource    gizmo_triangle{};

static Primitive *      gizmo_prim[size_t(GizmoShape::RANGE_SIZE)]{};
VK_NAMESPACE_END
