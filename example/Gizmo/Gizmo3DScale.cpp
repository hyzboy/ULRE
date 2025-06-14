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
#include<hgl/graph/SceneNode.h>
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
    Mesh *center_cube=GetGizmoMesh(GizmoShape::Cube,GizmoColor::White);
    Mesh *cylinder[3]
    {
        GetGizmoMesh(GizmoShape::Cylinder,GizmoColor::Red),
        GetGizmoMesh(GizmoShape::Cylinder,GizmoColor::Green),
        GetGizmoMesh(GizmoShape::Cylinder,GizmoColor::Blue),
    };

    Mesh *cube[3]
    {
        GetGizmoMesh(GizmoShape::Cube,GizmoColor::Red),
        GetGizmoMesh(GizmoShape::Cube,GizmoColor::Green),
        GetGizmoMesh(GizmoShape::Cube,GizmoColor::Blue),
    };

    Mesh *square[3]=
    {
        GetGizmoMesh(GizmoShape::Square,GizmoColor::Red),
        GetGizmoMesh(GizmoShape::Square,GizmoColor::Green),
        GetGizmoMesh(GizmoShape::Square,GizmoColor::Blue)
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
        
        root_node->Add(new SceneNode(scale(GIZMO_CENTER_SPHERE_RADIUS*2),center_cube));

        {
            Transform tm;

            const Vector3f one_scale(1);
            const Vector3f plane_scale(2);
            const Vector3f cylinder_scale(GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_HALF_LENGTH);

            {
                tm.SetScale(cylinder_scale);
                tm.SetTranslation(0,0,GIZMO_CYLINDER_OFFSET);
                root_node->Add(new SceneNode(tm,cylinder[2]));       //Z 向上圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(0,0,GIZMO_CONE_OFFSET);
                root_node->Add(new SceneNode(tm,cube[2]));           //Z 向上圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET,0);
                root_node->Add(new SceneNode(tm,square[2]));
            }

            {
                tm.SetScale(cylinder_scale);
                tm.SetRotation(AxisVector::Y,90);
                tm.SetTranslation(GIZMO_CYLINDER_OFFSET,0,0);
                root_node->Add(new SceneNode(tm,cylinder[0]));       //X 向右圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(GIZMO_CONE_OFFSET,0,0);
                root_node->Add(new SceneNode(tm,cube[0]));           //X 向右圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(0,GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET);
                root_node->Add(new SceneNode(tm,square[0]));
            }

            {
                tm.SetScale(cylinder_scale);
                tm.SetRotation(AxisVector::X,-90);
                tm.SetTranslation(0,GIZMO_CYLINDER_OFFSET,0);
                
                root_node->Add(new SceneNode(tm,cylinder[1]));       //Y 向前圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(0,GIZMO_CONE_OFFSET,0);
                root_node->Add(new SceneNode(tm,cube[1]));           //Y 向前圆锥

                tm.SetScale(plane_scale);
                tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,0,GIZMO_TWO_AXIS_OFFSET);
                root_node->Add(new SceneNode(tm,square[1]));
            }
        }

        sm_gizmo_scale=CreateGizmoStaticMesh(root_node);
    }

    if(!sm_gizmo_scale)
        return(false);

    return(true);
}

VK_NAMESPACE_END
