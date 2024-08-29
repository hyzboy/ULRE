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
    static StaticMesh *sm_gizmo_scale=nullptr;
}//namespace

StaticMesh *GetGizmoScaleStaticMesh()
{
    return sm_gizmo_scale;
}

void ClearGizmoScaleStaticMesh()
{
    SAFE_CLEAR(sm_gizmo_scale);
}

bool InitGizmoScaleStaticMesh()
{
    Renderable *center_cube=GetGizmoRenderable(GizmoShape::Cube,GizmoColor::White);
    Renderable *cylinder[3]
    {
        GetGizmoRenderable(GizmoShape::Cylinder,GizmoColor::Red),
        GetGizmoRenderable(GizmoShape::Cylinder,GizmoColor::Green),
        GetGizmoRenderable(GizmoShape::Cylinder,GizmoColor::Blue),
    };

    Renderable *cube[3]
    {
        GetGizmoRenderable(GizmoShape::Cube,GizmoColor::Red),
        GetGizmoRenderable(GizmoShape::Cube,GizmoColor::Green),
        GetGizmoRenderable(GizmoShape::Cube,GizmoColor::Blue),
    };

    Renderable *square[3]=
    {
        GetGizmoRenderable(GizmoShape::Square,GizmoColor::Red),
        GetGizmoRenderable(GizmoShape::Square,GizmoColor::Green),
        GetGizmoRenderable(GizmoShape::Square,GizmoColor::Blue)
    };

    if(!center_cube)
        return(false);

    for(int i=0;i<3;i++)
    {
        if(!cylinder[i])
            return(false);

        if(!cube[i])
            return(false);

        if(!square[i])
            return(false);
    }

    {
        SceneNode *root_node=new SceneNode();
        
        root_node->CreateSubNode(scale(GIZMO_CENTER_SPHERE_RADIUS*2),center_cube);

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
                root_node->CreateSubNode(tm,cube[2]);           //Z 向上圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET,0);
                root_node->CreateSubNode(tm,square[2]);
            }

            {
                tm.SetScale(cylinder_scale);
                tm.SetRotation(AxisVector::Y,90);
                tm.SetTranslation(GIZMO_CYLINDER_OFFSET,0,0);
                root_node->CreateSubNode(tm,cylinder[0]);       //X 向右圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(GIZMO_CONE_OFFSET,0,0);
                root_node->CreateSubNode(tm,cube[0]);           //X 向右圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(0,GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET);
                root_node->CreateSubNode(tm,square[0]);
            }

            {
                tm.SetScale(cylinder_scale);
                tm.SetRotation(AxisVector::X,-90);
                tm.SetTranslation(0,GIZMO_CYLINDER_OFFSET,0);
                
                root_node->CreateSubNode(tm,cylinder[1]);       //Y 向前圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(0,GIZMO_CONE_OFFSET,0);
                root_node->CreateSubNode(tm,cube[1]);           //Y 向前圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,0,GIZMO_TWO_AXIS_OFFSET);
                root_node->CreateSubNode(tm,square[1]);
            }
        }

        sm_gizmo_scale=CreateGizmoStaticMesh(root_node);
    }

    if(!sm_gizmo_scale)
        return(false);

    return(true);
}

VK_NAMESPACE_END
