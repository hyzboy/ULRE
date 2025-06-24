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
    /**
    * 移动 Gizmo 节点
    */
    class GizmoMoveNode:public SceneNode
    {
        struct GizmoMoveAxis
        {
            MeshComponent *cylinder;    //圆柱
            MeshComponent *cone;        //圆锥
            MeshComponent *square;      //双轴调节正方形
        };

        MeshComponent *sphere=nullptr;
        GizmoMoveAxis axis[3]{};  //X,Y,Z 三个轴

    public:

        using SceneNode::SceneNode;

        SceneNode *CreateNode()const override
        {
            return(new GizmoMoveNode);
        }

        bool Init(RenderFramework *render_framework)
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

            CreateComponentInfo cci(this);

            render_framework->CreateComponent<MeshComponent>(&cci,sphere);

            {
                Transform tm;

                const Vector3f one_scale(1);
                const Vector3f square_scale(2);
                const Vector3f cylinder_scale(GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_HALF_LENGTH);

                {
                    tm.SetScale(cylinder_scale);
                    tm.SetTranslation(0,0,GIZMO_CYLINDER_OFFSET);
                    cci.mat=tm;
                    render_framework->CreateComponent<MeshComponent>(&cci,cylinder[2]);       //Z 向上圆柱

                    tm.SetScale(one_scale);
                    tm.SetTranslation(0,0,GIZMO_CONE_OFFSET);
                    cci.mat=tm;
                    render_framework->CreateComponent<MeshComponent>(&cci,cone[2]);           //Z 向上圆锥

                    tm.SetScale(square_scale);
                    tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET,0);
                    cci.mat=tm;
                    render_framework->CreateComponent<MeshComponent>(&cci,square[2]);
                }

                {
                    tm.SetScale(cylinder_scale);
                    tm.SetRotation(AxisVector::Y,90);
                    tm.SetTranslation(GIZMO_CYLINDER_OFFSET,0,0);
                    cci.mat=tm;
                    render_framework->CreateComponent<MeshComponent>(&cci,cylinder[0]);       //X 向右圆柱

                    tm.SetScale(one_scale);
                    tm.SetTranslation(GIZMO_CONE_OFFSET,0,0);
                    cci.mat=tm;
                    render_framework->CreateComponent<MeshComponent>(&cci,cone[0]);           //X 向右圆锥

                    tm.SetScale(square_scale);
                    tm.SetTranslation(0,GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET);
                    cci.mat=tm;
                    render_framework->CreateComponent<MeshComponent>(&cci,square[0]);
                }

                {
                    tm.SetScale(cylinder_scale);
                    tm.SetRotation(AxisVector::X,-90);
                    tm.SetTranslation(0,GIZMO_CYLINDER_OFFSET,0);
                    cci.mat=tm;
                    render_framework->CreateComponent<MeshComponent>(&cci,cylinder[1]);       //Y 向前圆柱

                    tm.SetScale(one_scale);
                    tm.SetTranslation(0,GIZMO_CONE_OFFSET,0);
                    cci.mat=tm;
                    render_framework->CreateComponent<MeshComponent>(&cci,cone[1]);           //Y 向前圆锥

                    tm.SetScale(square_scale);
                    tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,0,GIZMO_TWO_AXIS_OFFSET);
                    cci.mat=tm;
                    render_framework->CreateComponent<MeshComponent>(&cci,square[1]);
                }
            }

            return(true);
        }
    };//class GizmoMoveNode:public SceneNode

    static GizmoMoveNode *sn_gizmo_move=nullptr;
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
    if(sn_gizmo_move)
        return(false);

    sn_gizmo_move=new GizmoMoveNode;

    if(!sn_gizmo_move->Init(render_framework))
    {
        delete sn_gizmo_move;
        sn_gizmo_move=nullptr;
        return(false);
    }

    return(true);
}

VK_NAMESPACE_END
