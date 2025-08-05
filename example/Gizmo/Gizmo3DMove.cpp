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
            MaterialInstance *mi;       //材质实例
            MeshComponent *cylinder;    //圆柱
            MeshComponent *cone;        //圆锥
            MeshComponent *square;      //双轴调节正方形
        };

        MaterialInstance *pick_mi;
        MeshComponent *sphere=nullptr;
        GizmoMoveAxis axis[3]{};  //X,Y,Z 三个轴

    protected:

        Ray     MouseRay;

        int     CurAXIS=-1;             //当前鼠标选中轴
        float   CurDist=0;              //当前距离

        int         PickAXIS=-1;        //拾取轴

        float       PickDist=0;         //拾取辆距离轴心的距离
        Matrix4f    PickL2W;            //拾取时的变换矩阵
        Matrix4f    PickW2L;            //拾取时的变换矩阵逆
        Vector3f    PickCenter;         //拾取时的中心位置

        TransformTranslate3f *CurTranslate=nullptr;

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

        void DuplicationComponents(SceneNode *node) const override
        {
            GizmoMoveNode *gmn=(GizmoMoveNode *)node;

            if(!gmn)
                return;

            gmn->pick_mi=pick_mi;

        #define DUPLICATION_COMPONENT(c)    gmn->c=(MeshComponent *)(c->Duplication()); \
                                            gmn->AttachComponent(gmn->c);

            DUPLICATION_COMPONENT(sphere);

            for(size_t i=0;i<3;i++)
            {
                gmn->axis[i].mi=axis[i].mi; // 复制材质实例
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

            pick_mi=GetGizmoMI3D(GizmoColor::Yellow); //获取拾取材质实例

            sphere=render_framework->CreateComponent<MeshComponent>(&cci,SpherePtr);        //中心球
            sphere->SetOverrideMaterial(GetGizmoMI3D(GizmoColor::White));                     //白色

            {
                Transform tm;
                GizmoMoveAxis *gma;

                const Vector3f one_scale(1);
                const Vector3f square_scale(2);
                const Vector3f cylinder_scale(GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_HALF_LENGTH);

                {
                    gma=axis+size_t(AXIS::Z);
                    gma->mi=GetGizmoMI3D(GizmoColor::Blue);

                    tm.SetScale(cylinder_scale);
                    tm.SetTranslation(0,0,GIZMO_CYLINDER_OFFSET);
                    cci.mat=tm;
                    gma->cylinder=render_framework->CreateComponent<MeshComponent>(&cci,CylinderPtr);       //Z 向上圆柱
                    gma->cylinder->SetOverrideMaterial(gma->mi);

                    tm.SetScale(one_scale);
                    tm.SetTranslation(0,0,GIZMO_CONE_OFFSET);
                    cci.mat=tm;
                    gma->cone=render_framework->CreateComponent<MeshComponent>(&cci,ConePtr);           //Z 向上圆锥
                    gma->cone->SetOverrideMaterial(gma->mi);

                    tm.SetScale(square_scale);
                    tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET,0);
                    cci.mat=tm;
                    gma->square=render_framework->CreateComponent<MeshComponent>(&cci,SquarePtr);
                    gma->square->SetOverrideMaterial(gma->mi);
                }

                {
                    gma=axis+size_t(AXIS::X);
                    gma->mi=GetGizmoMI3D(GizmoColor::Red);

                    tm.SetScale(cylinder_scale);
                    tm.SetRotation(AxisVector::Y,90);
                    tm.SetTranslation(GIZMO_CYLINDER_OFFSET,0,0);
                    cci.mat=tm;
                    gma->cylinder=render_framework->CreateComponent<MeshComponent>(&cci,CylinderPtr);       //X 向右圆柱
                    gma->cylinder->SetOverrideMaterial(gma->mi);

                    tm.SetScale(one_scale);
                    tm.SetTranslation(GIZMO_CONE_OFFSET,0,0);
                    cci.mat=tm;
                    gma->cone=render_framework->CreateComponent<MeshComponent>(&cci,ConePtr);           //Z 向上圆锥
                    gma->cone->SetOverrideMaterial(gma->mi);

                    tm.SetScale(square_scale);
                    tm.SetTranslation(0,GIZMO_TWO_AXIS_OFFSET,GIZMO_TWO_AXIS_OFFSET);
                    cci.mat=tm;
                    gma->square=render_framework->CreateComponent<MeshComponent>(&cci,SquarePtr);
                    gma->square->SetOverrideMaterial(gma->mi);
                }

                {
                    gma=axis+size_t(AXIS::Y);
                    gma->mi=GetGizmoMI3D(GizmoColor::Green);

                    tm.SetScale(cylinder_scale);
                    tm.SetRotation(AxisVector::X,-90);
                    tm.SetTranslation(0,GIZMO_CYLINDER_OFFSET,0);
                    cci.mat=tm;
                    gma->cylinder=render_framework->CreateComponent<MeshComponent>(&cci,CylinderPtr);       //X 向右圆柱
                    gma->cylinder->SetOverrideMaterial(gma->mi);

                    tm.SetScale(one_scale);
                    tm.SetTranslation(0,GIZMO_CONE_OFFSET,0);
                    cci.mat=tm;
                    gma->cone=render_framework->CreateComponent<MeshComponent>(&cci,ConePtr);           //Z 向上圆锥
                    gma->cone->SetOverrideMaterial(gma->mi);

                    tm.SetScale(square_scale);
                    tm.SetTranslation(GIZMO_TWO_AXIS_OFFSET,0,GIZMO_TWO_AXIS_OFFSET);
                    cci.mat=tm;
                    gma->square=render_framework->CreateComponent<MeshComponent>(&cci,SquarePtr);
                    gma->square->SetOverrideMaterial(gma->mi);
                }
            }

            return(true);
        }

        io::EventProcResult OnPressed(const Vector2i &mp,io::MouseButton mb) override
        {
            GizmoMoveNode::OnMove(mp);

            if(CurAXIS>=0&&CurAXIS<3)
            {
                PickAXIS    =CurAXIS;
                PickDist    =CurDist;

                if(CurTranslate)
                {
                    CurTranslate->SetOffset(Vector3f(0,0,0));
                }
                else
                {
                    CurTranslate=GetTransform().AddTranslate(Vector3f(0,0,0));
                }

                return io::EventProcResult::Break; // 处理完鼠标按下事件，停止进一步处理
            }

            return io::EventProcResult::Continue;
        }

        io::EventProcResult OnReleased(const Vector2i &,io::MouseButton mb) override
        {
            if(CurTranslate)
            {
                GetTransform().RemoveTransform(CurTranslate);
                CurTranslate=nullptr;
            }

            return io::EventProcResult::Continue;
        }

        io::EventProcResult OnMoveAtControl(const Vector2i &mouse_coord)
        {
            CameraControl *cc=GetCameraControl();

            if(!cc)
                return io::EventProcResult::Continue;

            cc->SetMouseRay(&MouseRay,mouse_coord);

            const CameraInfo *ci=cc->GetCameraInfo();

            const Matrix4f l2w=GetLocalToWorldMatrix();

            Vector3f axis_vector=GetAxisVector(AXIS(PickAXIS)); //取得轴向量

            Vector3f start=axis_vector*std::abs(ci->zfar-ci->znear);
            Vector3f end  =-start;

            start=TransformPosition(PickL2W,start); //将轴的终点转换到世界坐标
            end  =TransformPosition(PickL2W,end);   //将轴的终点转换到世界坐标

            Vector3f p_ray,p_ls;

            MouseRay.ClosestPoint(
                p_ray,          // 射线上的点
                p_ls,           // 线段上的点
                start,          // 线段起点
                end);           // 线段终点

            const Vector3f p_ls_local=TransformPosition(PickW2L,p_ls); //将线段上的点转换到本地坐标系

            CurDist=glm::length(p_ls)-PickDist;

            CurTranslate->SetOffset(axis_vector*CurDist);                            //设置偏移量

            return io::EventProcResult::Continue;
        }

        io::EventProcResult OnMove(const Vector2i &mouse_coord) override
        {
            if(CurTranslate)
            {
                return OnMoveAtControl(mouse_coord); //已经选中一个轴了
            }

            CameraControl *cc=GetCameraControl();

            if(!cc)
                return io::EventProcResult::Continue;

            cc->SetMouseRay(&MouseRay,mouse_coord);

            const CameraInfo *ci=cc->GetCameraInfo();
            const Matrix4f l2w=GetLocalToWorldMatrix();
            const Matrix4f w2l=GetInverseLocalToWorldMatrix();
            const Vector3f center=TransformPosition(l2w,Vector3f(0,0,0));
            const float center_ppu=cc->GetPixelPerUnit(center); //求原点坐标相对屏幕空间象素的缩放比
            Vector3f axis_vector;
            Vector3f start;
            Vector3f end;
            Vector3f p_ray,p_ls;
            float dist;
            float to_center_dist;
            float pixel_per_unit;
            MaterialInstance *mi;
            const float MaxViewLength=std::fabs(ci->zfar-ci->znear);
            const float axis_sphere_radius=glm::length(AxisVector::X*(GIZMO_CENTER_SPHERE_RADIUS/2.0f));
            const float axis_length=glm::length(AxisVector::X*GIZMO_ARROW_LENGTH)-axis_sphere_radius;

            CurAXIS=-1;

            for(int i=0;i<3;i++)
            {
                axis_vector=GetAxisVector(AXIS(i)); //取得轴向量

                start   =TransformPosition(l2w,axis_vector*GIZMO_CENTER_SPHERE_RADIUS); //将轴的起点转换到世界坐标
                end     =TransformPosition(l2w,axis_vector*MaxViewLength);

                //求射线与线段的最近点
                MouseRay.ClosestPoint(  p_ray,         //射线上的点
                                        p_ls,          //线段上的点
                                        start,end);    //线段

                //求p_ls坐标相对屏幕空间象素的缩放比
                pixel_per_unit=cc->GetPixelPerUnit(p_ls);
                to_center_dist=glm::distance(p_ls,center)/pixel_per_unit;        //到中心点的距离

                if(i==0)
                {
                    std::cout<<"line segment ppu: "<<pixel_per_unit<<std::endl;
                    std::cout<<"to center dist: "<<to_center_dist<<std::endl;
                }

                dist=glm::distance(p_ray,p_ls);         //计算射线与线段的距离

                if(i==0)
                {
                    std::cout<<"ray to line dist: "<<dist<<std::endl;
                }

                if(to_center_dist>axis_sphere_radius
                 &&to_center_dist<axis_length
                 &&dist<GIZMO_CYLINDER_RADIUS*pixel_per_unit)
                {
                    mi=pick_mi;

                    CurAXIS=i;

                    PickL2W=l2w;     //记录拾取时的变换矩阵
                    PickW2L=w2l;

                    p_ls=TransformPosition(w2l,p_ls);   //将线段上的点转换到本地坐标系

                    CurDist=glm::length(p_ls);          //计算线段上的点与原点的距离
                }
                else
                {
                    mi=axis[i].mi;
                }

                axis[i].cylinder->SetOverrideMaterial(mi);
                axis[i].cone->SetOverrideMaterial(mi);

                //std::cout<<"Mouse:    "<<mouse_coord.x<<","<<mouse_coord.y<<std::endl;
                //std::cout<<"Ray(Ori): "<<MouseRay.origin.x<<","<<MouseRay.origin.y<<","<<MouseRay.origin.z<<")"<<std::endl;
                //std::cout<<"Ray(Dir): "<<MouseRay.direction.x<<","<<MouseRay.direction.y<<","<<MouseRay.direction.z<<")"<<std::endl;
                //std::cout<<"Distance: "<<dist<<std::endl;
            }

            if(CurTranslate)
                return io::EventProcResult::Break;

            return io::EventProcResult::Continue;
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
