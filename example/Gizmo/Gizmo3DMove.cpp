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

        SceneNode * Duplication() const override
        {
            GizmoMoveNode *new_gmn=(GizmoMoveNode *)SceneNode::Duplication();

            if(!new_gmn)
                return(nullptr);

            new_gmn->sphere=sphere;
            hgl_cpy(new_gmn->axis,axis);

            return new_gmn;
        }

        bool Init(RenderFramework *render_framework)
        {
            ComponentDataPtr sphere_ptr=GetGizmoMeshCDP(GizmoShape::Sphere,GizmoColor::White);

        #define GET_GIZMO_MESH_CDP(shape) ComponentDataPtr shape##Ptr[3]{ \
                GetGizmoMeshCDP(GizmoShape::shape,GizmoColor::Red), \
                GetGizmoMeshCDP(GizmoShape::shape,GizmoColor::Green), \
                GetGizmoMeshCDP(GizmoShape::shape,GizmoColor::Blue) \
            };

            GET_GIZMO_MESH_CDP(Cylinder)
            GET_GIZMO_MESH_CDP(Cone)
            GET_GIZMO_MESH_CDP(Square)

            if(!sphere_ptr)
                return(false);

            for(int i=0;i<3;i++)
            {
                if(!CylinderPtr[i])
                    return(false);

                if(!ConePtr[i])
                    return(false);

                if(!SquarePtr[i])
                    return(false);
            }

            CreateComponentInfo cci(this);

            sphere=render_framework->CreateComponent<MeshComponent>(&cci,sphere_ptr);

            {
                Transform tm;
                GizmoMoveAxis *gma;

                const Vector3f one_scale(1);
                const Vector3f square_scale(2);
                const Vector3f cylinder_scale(GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_HALF_LENGTH);

                {
                    gma=axis+size_t(AXIS::Z);

                    tm.SetScale(cylinder_scale);
                    tm.SetTranslation(0,0,GIZMO_CYLINDER_OFFSET);
                    cci.mat=tm;
                    gma->cylinder=render_framework->CreateComponent<MeshComponent>(&cci,CylinderPtr[2]);       //Z 向上圆柱

                    tm.SetScale(one_scale);
                    tm.SetTranslation(0,0,GIZMO_CONE_OFFSET);
                    cci.mat=tm;
                    gma->cone=render_framework->CreateComponent<MeshComponent>(&cci,ConePtr[2]);           //Z 向上圆锥

                    tm.SetScale(square_scale);
                    tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET,0);
                    cci.mat=tm;
                    gma->square=render_framework->CreateComponent<MeshComponent>(&cci,SquarePtr[2]);
                }

                {
                    gma=axis+size_t(AXIS::X);

                    tm.SetScale(cylinder_scale);
                    tm.SetRotation(AxisVector::Y,90);
                    tm.SetTranslation(GIZMO_CYLINDER_OFFSET,0,0);
                    cci.mat=tm;
                    gma->cylinder=render_framework->CreateComponent<MeshComponent>(&cci,CylinderPtr[0]);       //X 向右圆柱

                    tm.SetScale(one_scale);
                    tm.SetTranslation(GIZMO_CONE_OFFSET,0,0);
                    cci.mat=tm;
                    gma->cone=render_framework->CreateComponent<MeshComponent>(&cci,ConePtr[0]);           //X 向右圆锥

                    tm.SetScale(square_scale);
                    tm.SetTranslation(0,GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET);
                    cci.mat=tm;
                    gma->square=render_framework->CreateComponent<MeshComponent>(&cci,SquarePtr[0]);
                }

                {
                    gma=axis+size_t(AXIS::Y);

                    tm.SetScale(cylinder_scale);
                    tm.SetRotation(AxisVector::X,-90);
                    tm.SetTranslation(0,GIZMO_CYLINDER_OFFSET,0);
                    cci.mat=tm;
                    gma->cylinder=render_framework->CreateComponent<MeshComponent>(&cci,CylinderPtr[1]);       //Y 向前圆柱

                    tm.SetScale(one_scale);
                    tm.SetTranslation(0,GIZMO_CONE_OFFSET,0);
                    cci.mat=tm;
                    gma->cone=render_framework->CreateComponent<MeshComponent>(&cci,ConePtr[1]);           //Y 向前圆锥

                    tm.SetScale(square_scale);
                    tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,0,GIZMO_TWO_AXIS_OFFSET);
                    cci.mat=tm;
                    gma->square=render_framework->CreateComponent<MeshComponent>(&cci,SquarePtr[1]);
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
