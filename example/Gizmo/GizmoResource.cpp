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

    static Material *          material =nullptr;
    static MaterialInstance *  mtl_inst[size_t(GizmoColor::RANGE_SIZE)]{};
    static Pipeline *          pipeline =nullptr;
    static VertexDataManager * vdm      =nullptr;

    static PrimitiveCreater *  prim_creater=nullptr;

    static Primitive *         prim[size_t(GizmoShape::RANGE_SIZE)]{};

    bool InitGizmoMI(RenderResource *rr)
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

        if(!rr||!material)
            return(false);

        Color4f color;

        for(uint i=0;i<uint(GizmoColor::RANGE_SIZE);i++)
        {
            color=GetColor4f(gizmo_color[i],1.0);

            mtl_inst[i]=rr->CreateMaterialInstance(material,nullptr,&color);
            if(!mtl_inst[i])
                return(false);
        }

        return(true);
    }
}//namespace

bool InitGizmoResource(RenderResource *rr)
{
    if(!rr)
        return(false);

    GPUDevice *device=rr->GetDevice();
    RenderPass *render_pass=device->GetRenderPass();

    {
        mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"Gizmo3D",Prim::Triangles);

        cfg.local_to_world=true;
        cfg.material_instance=true;

        mtl::MaterialCreateInfo *mci=CreateMaterialGizmo3D(&cfg);

        if(!mci)
            return(false);

        material=rr->CreateMaterial(mci);
        if(!material)
            return(false);
    }

    if(!InitGizmoMI(rr))
        return(false);

    {
        pipeline=render_pass->CreatePipeline(material,InlinePipeline::Solid3D,Prim::Triangles);
        if(!pipeline)
            return(false);
    }

    {
        vdm=new VertexDataManager(device,material->GetDefaultVIL());

        if(!vdm)
            return(false);

        if(!vdm->Init(  HGL_SIZE_1MB,       //最大顶点数量
                        HGL_SIZE_1MB,       //最大索引数量
                        IndexType::U16))    //索引类型
            return(false);
    }

    {
        prim_creater=new PrimitiveCreater(vdm);

        if(!prim_creater)
            return(false);
    }

    {
        using namespace inline_geometry;

        {
            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.Set(32,32);
            pgci.sub_count.Set(8,8);

            pgci.lum=0.75;
            pgci.sub_lum=1.0;

            PrimitiveCreater pc(device,material->GetDefaultVIL());

            prim[size_t(GizmoShape::Plane)]=CreatePlaneGrid(prim_creater,&pgci);
        }
    }
    return(true);
}

VK_NAMESPACE_END