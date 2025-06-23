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
#include<hgl/graph/RenderFramework.h>
#include<hgl/component/MeshComponent.h>

VK_NAMESPACE_BEGIN

namespace
{
    ///**
    //* 移动 Gizmo 节点
    //*/
    //class GizmoMoveNode:public SceneNode
    //{
    //    struct GizmoMoveAxis
    //    {
    //        MeshComponent *cylinder =nullptr;   //圆柱
    //        MeshComponent *cone     =nullptr;   //圆锥
    //        MeshComponent *square   =nullptr;   //双轴调节正方形
    //    };

    //    MeshComponent *sphere=nullptr;
    //    GizmoMoveAxis axis[3];  //X,Y,Z 三个轴

    //public:

    //    GizmoMoveNode(RenderFramework *rf)
    //    {
    //        //SetName("GizmoMoveNode");

    //        Mesh *sphere=GetGizmoMesh(GizmoShape::Sphere,GizmoColor::White);

    //        sn_gizmo_move->AttachComponent(rf->CreateComponent<MeshComponent>(sphere));

    //        for(int i=0;i<3;i++)
    //        {
    //            axis[i].cylinder=nullptr;
    //            axis[i].cone=nullptr;
    //            axis[i].square=nullptr;
    //        }
    //    }


    //};//class GizmoMoveNode:public SceneNode

    static SceneNode *sn_gizmo_move=nullptr;
}//namespace

SceneNode *GetGizmoMoveNode()
{
    return sn_gizmo_move;
}

void ClearGizmoMoveNode()
{
    SAFE_CLEAR(sn_gizmo_move);
}

bool InitGizmoMoveNode(RenderFramework *render_framework)
{
    ComponentDataPtr sphere=GetGizmoMeshComponentDataPtr(GizmoShape::Sphere,GizmoColor::White);

    ComponentDataPtr cylinder[3]
    {
        GetGizmoMeshComponentDataPtr(GizmoShape::Cylinder,GizmoColor::Red),
        GetGizmoMeshComponentDataPtr(GizmoShape::Cylinder,GizmoColor::Green),
        GetGizmoMeshComponentDataPtr(GizmoShape::Cylinder,GizmoColor::Blue),
    };

    ComponentDataPtr cone[3]
    {
        GetGizmoMeshComponentDataPtr(GizmoShape::Cone,GizmoColor::Red),
        GetGizmoMeshComponentDataPtr(GizmoShape::Cone,GizmoColor::Green),
        GetGizmoMeshComponentDataPtr(GizmoShape::Cone,GizmoColor::Blue),
    };

    ComponentDataPtr square[3]=
    {
        GetGizmoMeshComponentDataPtr(GizmoShape::Square,GizmoColor::Red),
        GetGizmoMeshComponentDataPtr(GizmoShape::Square,GizmoColor::Green),
        GetGizmoMeshComponentDataPtr(GizmoShape::Square,GizmoColor::Blue)
    };

    if(!sphere)
        return(false);

    for(int i=0;i<3;i++)
    {
        if(!cylinder[i])
            return(false);

        if(!cone[i])
            return(false);

        if(!square[i])
            return(false);
    }

    {
        sn_gizmo_move=new SceneNode();

        sn_gizmo_move->AttachComponent(render_framework->CreateComponent<MeshComponent>(sphere));

        {
            Transform tm;

            const Vector3f one_scale(1);
            const Vector3f square_scale(2);
            const Vector3f cylinder_scale(GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_HALF_LENGTH);

            {
                tm.SetScale(cylinder_scale);
                tm.SetTranslation(0,0,GIZMO_CYLINDER_OFFSET);
                render_framework->CreateComponent<MeshComponent>(tm.GetMatrix(),sn_gizmo_move,cylinder[2]);       //Z 向上圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(0,0,GIZMO_CONE_OFFSET);
                render_framework->CreateComponent<MeshComponent>(tm.GetMatrix(),sn_gizmo_move,cone[2]);           //Z 向上圆锥

                tm.SetScale(square_scale);
                tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET,0);
                render_framework->CreateComponent<MeshComponent>(tm.GetMatrix(),sn_gizmo_move,square[2]);
            }

            {
                tm.SetScale(cylinder_scale);
                tm.SetRotation(AxisVector::Y,90);
                tm.SetTranslation(GIZMO_CYLINDER_OFFSET,0,0);
                render_framework->CreateComponent<MeshComponent>(tm.GetMatrix(),sn_gizmo_move,cylinder[0]);       //X 向右圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(GIZMO_CONE_OFFSET,0,0);
                render_framework->CreateComponent<MeshComponent>(tm.GetMatrix(),sn_gizmo_move,cone[0]);           //X 向右圆锥

                tm.SetScale(square_scale);
                tm.SetTranslation(0,GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET);
                render_framework->CreateComponent<MeshComponent>(tm.GetMatrix(),sn_gizmo_move,square[0]);
            }

            {
                tm.SetScale(cylinder_scale);
                tm.SetRotation(AxisVector::X,-90);
                tm.SetTranslation(0,GIZMO_CYLINDER_OFFSET,0);                
                render_framework->CreateComponent<MeshComponent>(tm.GetMatrix(),sn_gizmo_move,cylinder[1]);       //Y 向前圆柱

                tm.SetScale(one_scale);
                tm.SetTranslation(0,GIZMO_CONE_OFFSET,0);
                render_framework->CreateComponent<MeshComponent>(tm.GetMatrix(),sn_gizmo_move,cone[1]);           //Y 向前圆锥

                tm.SetScale(square_scale);
                tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,0,GIZMO_TWO_AXIS_OFFSET);
                render_framework->CreateComponent<MeshComponent>(tm.GetMatrix(),sn_gizmo_move,square[1]);
            }
        }
    }

    if(!sn_gizmo_move)
        return(false);

    return(true);
}

VK_NAMESPACE_END
