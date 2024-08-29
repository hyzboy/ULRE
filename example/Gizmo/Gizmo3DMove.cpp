/*
 Gizmo move

 ref: Blender 4

 
        0                 9-10 
        *----------------->>>>
        |
        |
        |         5+
        |         +6
        |
        |
        v

        假设轴尺寸为10
        箭头长度为2，直径为2
        双轴调节正方形，长宽为1，位置为5,5

        中心球半径为1
*/

#include"GizmoResource.h"
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/InlineGeometry.h>

VK_NAMESPACE_BEGIN

namespace
{
    static StaticMesh *sm_gizmo_move=nullptr;
}//namespace

StaticMesh *GetGizmoMoveStaticMesh()
{
    return sm_gizmo_move;
}

void ClearGizmoMoveStaticMesh()
{
    SAFE_CLEAR(sm_gizmo_move);
}

bool InitGizmoMoveStaticMesh()
{
    Renderable *sphere=GetGizmoRenderable(GizmoShape::Sphere,GizmoColor::White);
    Renderable *cylinder[3]
    {
        GetGizmoRenderable(GizmoShape::Cylinder,GizmoColor::Red),
        GetGizmoRenderable(GizmoShape::Cylinder,GizmoColor::Green),
        GetGizmoRenderable(GizmoShape::Cylinder,GizmoColor::Blue),
    };

    Renderable *cone[3]
    {
        GetGizmoRenderable(GizmoShape::Cone,GizmoColor::Red),
        GetGizmoRenderable(GizmoShape::Cone,GizmoColor::Green),
        GetGizmoRenderable(GizmoShape::Cone,GizmoColor::Blue),
    };

    Renderable *plane[3]=
    {
        GetGizmoRenderable(GizmoShape::Plane,GizmoColor::Red),
        GetGizmoRenderable(GizmoShape::Plane,GizmoColor::Green),
        GetGizmoRenderable(GizmoShape::Plane,GizmoColor::Blue)
    };

    if(!sphere)
        return(false);

    for(int i=0;i<3;i++)
    {
        if(!cylinder[i])
            return(false);

        if(!cone[i])
            return(false);

        if(!plane[i])
            return(false);
    }

    {
        SceneNode *root_node=new SceneNode(scale(1,1,2));       //.////// XY有效，Z无效。。。在查为什么
        
        root_node->CreateSubNode(sphere);

        {
            Transform tm;

            const Vector3f one_scale(1);
            const Vector3f plane_scale(2);
            const Vector3f cylinder_scale(0.35f,0.35f,4.0f);

            {
                tm.SetScale(cylinder_scale);
                tm.SetTranslation(0,0,4.5f);
                root_node->CreateSubNode(tm,cylinder[2]);       //Z 向上圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(0,0,9.5f);
                root_node->CreateSubNode(tm,cone[2]);           //Z 向上圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(5,5,0);
                root_node->CreateSubNode(tm,plane[2]);
            }

            {
                tm.SetScale(cylinder_scale);
                tm.SetRotation(AxisVector::Y,90);
                tm.SetTranslation(4.5f,0,0);
                root_node->CreateSubNode(tm,cylinder[0]);       //X 向右圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(9.5f,0,0);
                root_node->CreateSubNode(tm,cone[0]);           //X 向右圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(0,5,5);
                root_node->CreateSubNode(tm,plane[0]);
            }

            {
                tm.SetScale(cylinder_scale);
                tm.SetRotation(AxisVector::X,-90);
                tm.SetTranslation(0,4.5f,0);
                
                root_node->CreateSubNode(tm,cylinder[1]);       //Y 向前圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(0,9.5f,0);
                root_node->CreateSubNode(tm,cone[1]);           //Y 向前圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(5,0,5);
                root_node->CreateSubNode(tm,plane[1]);
            }
        }

        sm_gizmo_move=CreateGizmoStaticMesh(root_node);
    }

    if(!sm_gizmo_move)
        return(false);

    return(true);
}

VK_NAMESPACE_END
