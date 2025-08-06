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

VK_NAMESPACE_BEGIN

namespace
{
//    #define GIZMO_MOVE_SQUARE 

    const Vector3f one_scale(1.0f);
    const Vector3f square_scale(2.0f);
    const Vector3f cylinder_scale(GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_RADIUS,GIZMO_CYLINDER_HALF_LENGTH);

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

        #ifdef GIZMO_MOVE_SQUARE
            MeshComponent *square;      //双轴调节正方形
        #endif//GIZMO_MOVE_SQUARE
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

            #ifdef GIZMO_MOVE_SQUARE
                DUPLICATION_COMPONENT(axis[i].square);
            #endif//GIZMO_MOVE_SQUARE
            }

        #undef DUPLICATION_COMPONENT
        }

        bool CreateGizmoGeometry(RenderFramework *render_framework)
        {
            ComponentDataPtr SpherePtr  =GetGizmoMeshCDP(GizmoShape::Sphere);
            if(!SpherePtr   )return(false);

            ComponentDataPtr CylinderPtr=GetGizmoMeshCDP(GizmoShape::Cylinder);
            if(!CylinderPtr )return(false);

            ComponentDataPtr ConePtr    =GetGizmoMeshCDP(GizmoShape::Cone);
            if(!ConePtr     )return(false);

        #ifdef GIZMO_MOVE_SQUARE
            ComponentDataPtr SquarePtr  =GetGizmoMeshCDP(GizmoShape::Square);
            if(!SquarePtr   )return(false);
        #endif//GIZMO_MOVE_SQUARE

            CreateComponentInfo cci(this);

            pick_mi=GetGizmoMI3D(GizmoColor::Yellow); //获取拾取材质实例

            sphere=render_framework->CreateComponent<MeshComponent>(&cci,SpherePtr);        //中心球
            sphere->SetOverrideMaterial(GetGizmoMI3D(GizmoColor::White));                     //白色

            {
                struct
                {
                    //圆柱模型是向上的，所以要旋转
                    Vector3f    RotationAxis;
                    float       RotationAngle;

                    GizmoColor  Color;
                }
                GizmoAxisDrawConfig[3]=
                {
                    {AxisVector::Y, 90,GizmoColor::Red},
                    {AxisVector::X,-90,GizmoColor::Green},
                    {Vector3f(0,0,0),0,GizmoColor::Blue}
                };

                Transform tm;
                GizmoMoveAxis *gma=axis;
                Vector3f axis_vector;

            #ifdef GIZMO_MOVE_SQUARE
                Vector3f inv_axis_vector;
            #endif//GIZMO_MOVE_SQUARE

                const auto *cfg=GizmoAxisDrawConfig;

                for(int i=0;i<3;i++)
                {
                    gma->mi=GetGizmoMI3D(cfg->Color);

                    axis_vector=GetAxisVector(AXIS(i)); //取得轴向量

                    tm.SetScale(cylinder_scale);
                    tm.SetRotation(cfg->RotationAxis,cfg->RotationAngle);
                    tm.SetTranslation(axis_vector*GIZMO_CYLINDER_OFFSET);
                    cci.mat=tm;
                    gma->cylinder=render_framework->CreateComponent<MeshComponent>(&cci,CylinderPtr);       //X 向右圆柱
                    gma->cylinder->SetOverrideMaterial(gma->mi);

                    tm.SetScale(one_scale);
                    tm.SetTranslation(axis_vector*GIZMO_CONE_OFFSET);
                    cci.mat=tm;
                    gma->cone=render_framework->CreateComponent<MeshComponent>(&cci,ConePtr);           //Z 向上圆锥
                    gma->cone->SetOverrideMaterial(gma->mi);

                #ifdef GIZMO_MOVE_SQUARE
                    inv_axis_vector=Vector3f(1,1,1)-axis_vector;

                    tm.SetScale(square_scale);
                    tm.SetTranslation(inv_axis_vector*GIZMO_TWO_AXIS_OFFSET);
                    cci.mat=tm;
                    gma->square=render_framework->CreateComponent<MeshComponent>(&cci,SquarePtr);
                    gma->square->SetOverrideMaterial(gma->mi);
                #endif//GIZMO_MOVE_SQUARE

                    ++cfg;
                    ++gma;
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
                const Matrix4f new_l2w=GetLocalToWorldMatrix();

                ClearTransforms(); //清除所有变换

                SetLocalMatrix(new_l2w); //设置新的变换矩阵

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

            const Vector3f axis_vector=GetAxisVector(AXIS(PickAXIS)); //取得轴向量

            Vector3f p1= axis_vector*std::fabs(ci->zfar-ci->znear);
            Vector3f p2=-p1;

            p1=TransformPosition(PickL2W,p1);
            p2=TransformPosition(PickL2W,p2);

            Vector3f p_ray,p_ls;

            MouseRay.ClosestPoint(
                p_ray,          // 射线上的点
                p_ls,           // 线段上的点
                p1,p2);         // 线段

            CurDist=glm::dot(p_ls-PickCenter,axis_vector);              //计算偏移量

            CurTranslate->SetOffset(axis_vector*(CurDist-PickDist));    //设置偏移量

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
            const Vector3f center=TransformPosition(l2w,Vector3f(0,0,0));
            Vector3f axis_vector;
            Vector3f axis_endpoint;
            Vector3f p_ray,p_ls;
            float dist;
            float to_center_dist;
            MaterialInstance *mi;

            const float axis_sphere_radius  =glm::length(AxisVector::X*(GIZMO_CENTER_SPHERE_RADIUS/2.0f));
            const float axis_length         =glm::length(AxisVector::X*GIZMO_ARROW_LENGTH);

            CurAXIS=-1;

            for(int i=0;i<3;i++)
            {
                axis_vector=GetAxisVector(AXIS(i)); //取得轴向量

                axis_endpoint=TransformPosition(l2w,axis_vector*axis_length);

                //求射线与线段的最近点
                MouseRay.ClosestPoint(  p_ray,          //射线上的点
                                        p_ls,           //线段上的点
                                        center,         //中心点
                                        axis_endpoint); //轴射线终点

                //求p_ls坐标相对屏幕空间象素的缩放比
                to_center_dist  =glm::distance(p_ls,center);    //到中心点的距离
                dist            =glm::distance(p_ls,p_ray);     //计算射线与线段的距离

                if(to_center_dist>axis_sphere_radius
                 &&to_center_dist<axis_length
                 &&dist<GIZMO_CYLINDER_RADIUS)
                {
                    mi=pick_mi;

                    CurAXIS=i;

                    PickCenter=center;  //记录拾取时的中心位置
                    PickL2W=l2w;        //记录拾取时的变换矩阵

                    CurDist=to_center_dist;
                }
                else
                {
                    mi=axis[i].mi;
                }

                axis[i].cylinder->SetOverrideMaterial(mi);
                axis[i].cone->SetOverrideMaterial(mi);
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
