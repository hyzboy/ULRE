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
        GetGizmoRenderable(GizmoShape::Square,GizmoColor::Red),
        GetGizmoRenderable(GizmoShape::Square,GizmoColor::Green),
        GetGizmoRenderable(GizmoShape::Square,GizmoColor::Blue)
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
        SceneNode *root_node=new SceneNode();
        
        root_node->CreateSubNode(sphere);

        {
            Transform tm;

            const Vector3f one_scale(1);
            const Vector3f plane_scale(2);
            const Vector3f cylinder_scale(GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_HALF_LENGTH);

            {
                tm.SetScale(cylinder_scale);
                tm.SetTranslation(0,0,GIZMO_CYLINDER_OFFSET);
                root_node->CreateSubNode(tm,cylinder[2]);       //Z 向上圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(0,0,GIZMO_CONE_OFFSET);
                root_node->CreateSubNode(tm,cone[2]);           //Z 向上圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET,0);
                root_node->CreateSubNode(tm,plane[2]);
            }

            {
                tm.SetScale(cylinder_scale);
                tm.SetRotation(AxisVector::Y,90);
                tm.SetTranslation(GIZMO_CYLINDER_OFFSET,0,0);
                root_node->CreateSubNode(tm,cylinder[0]);       //X 向右圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(GIZMO_CONE_OFFSET,0,0);
                root_node->CreateSubNode(tm,cone[0]);           //X 向右圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(0,GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET);
                root_node->CreateSubNode(tm,plane[0]);
            }

            {
                tm.SetScale(cylinder_scale);
                tm.SetRotation(AxisVector::X,-90);
                tm.SetTranslation(0,GIZMO_CYLINDER_OFFSET,0);
                
                root_node->CreateSubNode(tm,cylinder[1]);       //Y 向前圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(0,GIZMO_CONE_OFFSET,0);
                root_node->CreateSubNode(tm,cone[1]);           //Y 向前圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,0,GIZMO_TWO_AXIS_OFFSET);
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
