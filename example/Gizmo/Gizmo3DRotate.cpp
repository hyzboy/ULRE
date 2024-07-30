﻿#include"GizmoResource.h"
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/InlineGeometry.h>

VK_NAMESPACE_BEGIN

namespace
{
    static StaticMesh *sm_gizmo_rotate=nullptr;
}//namespace

StaticMesh *GetGizmoRotateStaticMesh()
{
    return sm_gizmo_rotate;
}

void ClearGizmoRotateStaticMesh()
{
    SAFE_CLEAR(sm_gizmo_rotate);
}

bool InitGizmoRotateStaticMesh()
{
    Renderable *torus[4]
    {
        GetGizmoRenderable(GizmoShape::Torus,GizmoColor::Red),
        GetGizmoRenderable(GizmoShape::Torus,GizmoColor::Green),
        GetGizmoRenderable(GizmoShape::Torus,GizmoColor::Blue),

        GetGizmoRenderable(GizmoShape::Torus,GizmoColor::White),
    };

    for(int i=0;i<4;i++)
    {
        if(!torus[i])
            return(false);
    }

    {
        SceneNode *root_node=new SceneNode();

        {
            Transform tm;

            tm.SetScale(10.0f);

            tm.SetRotation(AXIS::Z,90);
            root_node->CreateSubNode(tm,torus[0]);

            tm.ClearRotation();
            root_node->CreateSubNode(tm,torus[1]);

            tm.SetRotation(AXIS::X,90);
            root_node->CreateSubNode(tm,torus[2]);
        }

        sm_gizmo_rotate=CreateGizmoStaticMesh(root_node);
    }

    if(!sm_gizmo_rotate)
        return(false);

    return(true);
}

VK_NAMESPACE_END
