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
#include<hgl/graph/Scene.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/io/event/MouseEvent.h>
#include<hgl/graph/Ray.h>

#include<iostream>

VK_NAMESPACE_BEGIN

namespace
{
    /**
    * 移动 Gizmo 节点
    */
    class GizmoMoveNode:public SceneNode,io::MouseEvent
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

        io::EventDispatcher *GetEventDispatcher() override
        {
            return this; // GizmoMoveNode 处理鼠标事件
        }

        SceneNode *CreateNode()const override
        {
            return(new GizmoMoveNode);
        }

        void        DuplicationComponents(SceneNode *node) const override
        {
            GizmoMoveNode *gmn=(GizmoMoveNode *)node;
            if(!gmn)
                return;

        #define DUPLICATION_COMPONENT(c)    gmn->c=(MeshComponent *)(c->Duplication()); \
                                            gmn->AttachComponent(gmn->c);

            DUPLICATION_COMPONENT(sphere);

            for(size_t i=0;i<3;i++)
            {
                DUPLICATION_COMPONENT(axis[i].cylinder);
                DUPLICATION_COMPONENT(axis[i].cone);
                DUPLICATION_COMPONENT(axis[i].square);
            }

        #undef DUPLICATION_COMPONENT
        }

        bool CreateGizmoGeometry(RenderFramework *render_framework)
        {
            ComponentDataPtr SpherePtr  =GetGizmoMeshCDP(GizmoShape::Sphere);
            ComponentDataPtr CylinderPtr=GetGizmoMeshCDP(GizmoShape::Cylinder);
            ComponentDataPtr ConePtr    =GetGizmoMeshCDP(GizmoShape::Cone);
            ComponentDataPtr SquarePtr  =GetGizmoMeshCDP(GizmoShape::Square);

            if(!SpherePtr   )return(false);
            if(!CylinderPtr )return(false);
            if(!ConePtr     )return(false);
            if(!SquarePtr   )return(false);

            CreateComponentInfo cci(this);

            sphere=render_framework->CreateComponent<MeshComponent>(&cci,SpherePtr);        //中心球
            sphere->SetOverrideMaterial(GetGizmoMI3D(GizmoColor::White));                     //白色

            {
                Transform tm;
                GizmoMoveAxis *gma;
                MaterialInstance *mi=nullptr;

                const Vector3f one_scale(1);
                const Vector3f square_scale(2);
                const Vector3f cylinder_scale(GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_HALF_LENGTH);

                {
                    gma=axis+size_t(AXIS::Z);
                    mi=GetGizmoMI3D(GizmoColor::Blue);

                    tm.SetScale(cylinder_scale);
                    tm.SetTranslation(0,0,GIZMO_CYLINDER_OFFSET);
                    cci.mat=tm;
                    gma->cylinder=render_framework->CreateComponent<MeshComponent>(&cci,CylinderPtr);       //Z 向上圆柱
                    gma->cylinder->SetOverrideMaterial(mi);

                    tm.SetScale(one_scale);
                    tm.SetTranslation(0,0,GIZMO_CONE_OFFSET);
                    cci.mat=tm;
                    gma->cone=render_framework->CreateComponent<MeshComponent>(&cci,ConePtr);           //Z 向上圆锥
                    gma->cone->SetOverrideMaterial(mi);

                    tm.SetScale(square_scale);
                    tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET,0);
                    cci.mat=tm;
                    gma->square=render_framework->CreateComponent<MeshComponent>(&cci,SquarePtr);
                    gma->square->SetOverrideMaterial(mi);
                }

                {
                    gma=axis+size_t(AXIS::X);
                    mi=GetGizmoMI3D(GizmoColor::Red);

                    tm.SetScale(cylinder_scale);
                    tm.SetRotation(AxisVector::Y,90);
                    tm.SetTranslation(GIZMO_CYLINDER_OFFSET,0,0);
                    cci.mat=tm;
                    gma->cylinder=render_framework->CreateComponent<MeshComponent>(&cci,CylinderPtr);       //X 向右圆柱
                    gma->cylinder->SetOverrideMaterial(mi);

                    tm.SetScale(one_scale);
                    tm.SetTranslation(GIZMO_CONE_OFFSET,0,0);
                    cci.mat=tm;
                    gma->cone=render_framework->CreateComponent<MeshComponent>(&cci,ConePtr);           //Z 向上圆锥
                    gma->cone->SetOverrideMaterial(mi);

                    tm.SetScale(square_scale);
                    tm.SetTranslation(0,GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET);
                    cci.mat=tm;
                    gma->square=render_framework->CreateComponent<MeshComponent>(&cci,SquarePtr);
                    gma->square->SetOverrideMaterial(mi);
                }

                {
                    gma=axis+size_t(AXIS::Y);
                    mi=GetGizmoMI3D(GizmoColor::Green);

                    tm.SetScale(cylinder_scale);
                    tm.SetRotation(AxisVector::X,-90);
                    tm.SetTranslation(0,GIZMO_CYLINDER_OFFSET,0);
                    cci.mat=tm;
                    gma->cylinder=render_framework->CreateComponent<MeshComponent>(&cci,CylinderPtr);       //X 向右圆柱
                    gma->cylinder->SetOverrideMaterial(mi);

                    tm.SetScale(one_scale);
                    tm.SetTranslation(0,GIZMO_CONE_OFFSET,0);
                    cci.mat=tm;
                    gma->cone=render_framework->CreateComponent<MeshComponent>(&cci,ConePtr);           //Z 向上圆锥
                    gma->cone->SetOverrideMaterial(mi);

                    tm.SetScale(square_scale);
                    tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,0,GIZMO_TWO_AXIS_OFFSET);
                    cci.mat=tm;
                    gma->square=render_framework->CreateComponent<MeshComponent>(&cci,SquarePtr);
                    gma->square->SetOverrideMaterial(mi);
                }
            }

            return(true);
        }

        bool OnMove(const Vector2i &mouse_coord) override
        {
            CameraControl *cc=GetCameraControl();

            if(!cc)
                return(false);

            Ray ray;

            cc->SetMouseRay(&ray,mouse_coord);

            Matrix4f l2w=GetLocalToWorldMatrix();
            Vector3f start;
            Vector3f end;
            Vector3f p_ray,p_ls;
            float dist;
            float pixel_per_unit;

            start=TransformPosition(l2w,Vector3f(0,0,0)); //将原点转换到世界坐标

            {
                end=TransformPosition(l2w,Vector3f((GIZMO_CONE_LENGTH+GIZMO_CYLINDER_HALF_LENGTH)*20,0,0));

                //求射线与线段的最近点
                ray.ClosestPoint(p_ray,         //射线上的点
                                 p_ls,          //线段上的点
                                 start,end);    //线段

                dist=glm::distance(p_ray,p_ls); //计算射线与线段的距离

                //求p_ls坐标相对屏幕空间象素的缩放比
                pixel_per_unit=cc->GetPixelPerUnit(p_ls);

                MaterialInstance *mi=GetGizmoMI3D(dist<GIZMO_CYLINDER_RADIUS*pixel_per_unit?GizmoColor::Yellow:GizmoColor::Red);

                axis[size_t(AXIS::X)].cylinder->SetOverrideMaterial(mi);
                axis[size_t(AXIS::X)].cone->SetOverrideMaterial(mi);

                //std::cout<<"Mouse:    "<<mouse_coord.x<<","<<mouse_coord.y<<std::endl;
                //std::cout<<"Ray(Ori): "<<ray.origin.x<<","<<ray.origin.y<<","<<ray.origin.z<<")"<<std::endl;
                //std::cout<<"Ray(Dir): "<<ray.direction.x<<","<<ray.direction.y<<","<<ray.direction.z<<")"<<std::endl;
                //std::cout<<"Distance: "<<dist<<std::endl;
            }

            return false;
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

    if(!sn_gizmo_move->CreateGizmoGeometry(render_framework))
    {
        delete sn_gizmo_move;
        sn_gizmo_move=nullptr;
        return(false);
    }

    return(true);
}

VK_NAMESPACE_END
