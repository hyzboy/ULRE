// Billboard

#include<hgl/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/component/PrimitiveComponent.h>

using namespace hgl;
using namespace hgl::graph;

static float position_data[3]=
{
    0,0,0
};

static float lumiance_data[2]={1,1};

static Color4f white_color(1,1,1,1);
static Color4f yellow_color(1,1,0,1);

class TestApp:public WorkObject
{
    Color4f color;

private:

    Material *          mtl_plane_grid      =nullptr;
    MaterialInstance *  mi_plane_grid       =nullptr;
    Pipeline *          pipeline_plane_grid =nullptr;
    Geometry *          geom_plane_grid     =nullptr;

    MaterialInstance *  mi_billboard        =nullptr;
    Pipeline *          pipeline_billboard  =nullptr;
    Primitive *         prim_billboard      =nullptr;
    
    Texture2D *         texture             =nullptr;
    Sampler *           sampler             =nullptr;

private:

    bool InitPlaneGridMP()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);

        cfg.local_to_world=true;

        {
            cfg.position_format=VAT_VEC2;

            mtl_plane_grid=LoadMaterial("Std3D/VertexLum3D",&cfg);
            if(!mtl_plane_grid)return(false);

            VILConfig vil_config;

            vil_config.Add(VAN::Luminance,VF_V1UN8);
       
            mi_plane_grid=CreateMaterialInstance(mtl_plane_grid,&vil_config,&white_color);
            if(!mi_plane_grid)return(false);

            pipeline_plane_grid=CreatePipeline(mi_plane_grid,InlinePipeline::Solid3D);
            if(!pipeline_plane_grid)return(false);
        }

        return(true);
    }

    bool InitBillboardMP()
    {
        mtl::BillboardMaterialCreateConfig cfg(PrimitiveType::Billboard);

        {
            cfg.fixed_size=true;

            mi_billboard=CreateMaterialInstance(mtl::inline_material::Billboard2D,&cfg);
            if(!mi_billboard)return(false);

            pipeline_billboard=CreatePipeline(mi_billboard,InlinePipeline::Solid3D);
            if(!pipeline_billboard)return(false);
        }

        return(true);
    }

    bool InitTexture()
    {
        TextureManager *tex_manager=GetTextureManager();

        texture=tex_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"),true);
        if(!texture)return(false);

        sampler=CreateSampler();

        if(!mi_billboard->GetMaterial()->BindTextureSampler(DescriptorSetType::PerMaterial,     ///<描述符合集
                                                            mtl::SamplerName::BaseColor,        ///<采样器名称
                                                            texture,                            ///<纹理
                                                            sampler))                           ///<采样器
            return(false);

        Vector2u texture_size(texture->GetWidth(),texture->GetHeight());

        mi_billboard->WriteMIData(texture_size);

        return(true);
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        {
            auto pc=GetGeometryCreater(mi_plane_grid);

            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.Set(500,500);
            pgci.sub_count.Set(5,5);

            pgci.lum=128;
            pgci.sub_lum=192;

            geom_plane_grid=CreatePlaneGrid2D(pc,&pgci);
        }

        {
            auto pc=GetGeometryCreater(mi_billboard);

            pc->Init("Billboard",1);

            if(!pc->WriteVAB(VAN::Position,VF_V3F,position_data))
                return(false);

            prim_billboard=CreatePrimitive(pc,mi_billboard,pipeline_billboard);

            if(!prim_billboard)
                return(false);
        }

        return(true);
    }

    bool InitScene()
    {
        CreateComponentInfo cci(GetWorldRootNode());

        CreateComponent<PrimitiveComponent>(&cci,CreatePrimitive(geom_plane_grid,mi_plane_grid,pipeline_plane_grid));
        CreateComponent<PrimitiveComponent>(&cci,prim_billboard);

        CameraControl *camera_control=GetCameraControl();

        camera_control->SetPosition(Vector3f(32,32,32));
        camera_control->SetTarget(Vector3f(0,0,0));

        return(true);
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(geom_plane_grid);
    }

    bool Init() override
    {
        if(!InitPlaneGridMP())
            return(false);

        if(!InitBillboardMP())
            return(false);

        if(!InitTexture())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Billboard"),1280,720);
}
