#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/color/Color.h>
#include<hgl/graph/InlineGeometry.h>

VK_NAMESPACE_BEGIN

namespace
{
    enum class GizmoColor:uint
    {
        Black=0,
        White,

        Red,
        Green,
        Blue,

        Yellow,

        ENUM_CLASS_RANGE(Black,Yellow)
    };

    static Color4f GizmoColorRGB[size_t(GizmoColor::RANGE_SIZE)];

    enum class GizmoShape:uint
    {
        Plane=0,    //平面
        Cube,       //立方体
        Sphere,     //球
        Cone,       //圆锥
        Cylinder,   //圆柱

        ENUM_CLASS_RANGE(Plane,Cylinder)
    };

    static RenderResource *    gizmo_rr                 =nullptr;

    static Material *          gizmo_mtl_triangles      =nullptr;
    static MaterialInstance *  gizmo_mi_triangles[size_t(GizmoColor::RANGE_SIZE)]{};
    static Pipeline *          gizmo_pipeline_triangles =nullptr;
    static VertexDataManager * gizmo_vdm_triangles      =nullptr;

    static PrimitiveCreater *  gizmo_prim_creater       =nullptr;

    static Primitive *         gizmo_prim[size_t(GizmoShape::RANGE_SIZE)]{};

    bool InitGizmoMI()
    {
        constexpr const COLOR gizmo_color[size_t(GizmoColor::RANGE_SIZE)]=
        {
            COLOR::MozillaCharcoal,
            COLOR::BlanchedAlmond,

            COLOR::BlenderAxisRed,
            COLOR::BlenderAxisGreen,
            COLOR::BlenderAxisBlue,

            COLOR::BlenderYellow,
        };

        if(!gizmo_rr||!gizmo_mtl_triangles)
            return(false);

        Color4f color;

        for(uint i=0;i<uint(GizmoColor::RANGE_SIZE);i++)
        {
            color=GetColor4f(gizmo_color[i],1.0);

            gizmo_mi_triangles[i]=gizmo_rr->CreateMaterialInstance(gizmo_mtl_triangles,nullptr,&color);
            if(!gizmo_mi_triangles[i])
                return(false);
        }

        return(true);
    }
}//namespace

bool InitGizmoResource(GPUDevice *device)
{
    gizmo_rr=new RenderResource(device);

    if(!gizmo_rr)
        return(false);

    RenderPass *render_pass=device->GetRenderPass();

    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"Gizmo3D",Prim::Triangles);

        cfg.local_to_world=true;
        cfg.material_instance=true;

        mtl::MaterialCreateInfo *mci=CreateMaterialGizmo3D(&cfg);

        if(!mci)
            return(false);

        gizmo_mtl_triangles=gizmo_rr->CreateMaterial(mci);
        if(!gizmo_mtl_triangles)
            return(false);
    }

    if(!InitGizmoMI())
        return(false);

    {
        gizmo_pipeline_triangles=render_pass->CreatePipeline(gizmo_mtl_triangles,InlinePipeline::Solid3D,Prim::Triangles);
        if(!gizmo_pipeline_triangles)
            return(false);
    }

    {
        gizmo_vdm_triangles=new VertexDataManager(device,gizmo_mtl_triangles->GetDefaultVIL());

        if(!gizmo_vdm_triangles)
            return(false);

        if(!gizmo_vdm_triangles->Init(HGL_SIZE_1MB,       //最大顶点数量
                            HGL_SIZE_1MB,       //最大索引数量
                            IndexType::U16))    //索引类型
            return(false);
    }

    {
        gizmo_prim_creater=new PrimitiveCreater(gizmo_vdm_triangles);

        if(!gizmo_prim_creater)
            return(false);
    }

    {
        using namespace inline_geometry;

        {
            gizmo_prim[size_t(GizmoShape::Plane)]=CreatePlane(gizmo_prim_creater);
        }

        {
            CubeCreateInfo cci;

            cci.normal=true;
            cci.tangent=false;
            cci.tex_coord=false;

            gizmo_prim[size_t(GizmoShape::Cube)]=CreateCube(gizmo_prim_creater,&cci);
        }

        {
            gizmo_prim[size_t(GizmoShape::Sphere)]=CreateSphere(gizmo_prim_creater,8);
        }

        {
            ConeCreateInfo cci;

            cci.radius      =1;         //圆锥半径
            cci.halfExtend  =1;         //圆锤一半高度
            cci.numberSlices=8;        //圆锥底部分割数
            cci.numberStacks=1;         //圆锥高度分割数

            gizmo_prim[size_t(GizmoShape::Cone)]=CreateCone(gizmo_prim_creater,&cci);
        }

        {
            struct CylinderCreateInfo cci;

            cci.halfExtend  =1;         //圆柱一半高度
            cci.numberSlices=8;         //圆柱底部分割数
            cci.radius      =1;         //圆柱半径

            gizmo_prim[size_t(GizmoShape::Cylinder)]=CreateCylinder(gizmo_prim_creater,&cci);
        }

        ENUM_CLASS_FOR(GizmoShape,int,i)
        {
            if(!gizmo_prim[i])
                return(false);
        }
    }

    return(true);
}

void FreeGizmoResource()
{
    SAFE_CLEAR_OBJECT_ARRAY(gizmo_prim)
    SAFE_CLEAR(gizmo_prim_creater);
    SAFE_CLEAR(gizmo_vdm_triangles);
//    SAFE_CLEAR(gizmo_pipeline_triangles);
//    SAFE_CLEAR_OBJECT_ARRAY(gizmo_mi_triangles)
    //SAFE_CLEAR(gizmo_mtl_triangles);
    SAFE_CLEAR(gizmo_rr);
}

VK_NAMESPACE_END