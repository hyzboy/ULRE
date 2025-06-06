// Billboard

#include<hgl/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/Ray.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/FirstPersonCameraControl.h>

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
    Primitive *         prim_plane_grid     =nullptr;

    MaterialInstance *  mi_billboard        =nullptr;
    Pipeline *          pipeline_billboard  =nullptr;
    Mesh *              ro_billboard        =nullptr;
    
    Texture2D *         texture             =nullptr;
    Sampler *           sampler             =nullptr;

private:

    bool InitPlaneGridMP()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);

        cfg.local_to_world=true;

        {
            cfg.position_format=VAT_VEC2;

            mtl_plane_grid=db->LoadMaterial("Std3D/VertexLum3D",&cfg);
            if(!mtl_plane_grid)return(false);

            VILConfig vil_config;

            vil_config.Add(VAN::Luminance,VF_V1UN8);
       
            mi_plane_grid=db->CreateMaterialInstance(mtl_plane_grid,&vil_config,&white_color);
            if(!mi_plane_grid)return(false);

            pipeline_plane_grid=CreatePipeline(mi_plane_grid,InlinePipeline::Solid3D,PrimitiveType::Lines);
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

            pipeline_billboard=CreatePipeline(mi_billboard,InlinePipeline::Solid3D,PrimitiveType::Billboard);
            if(!pipeline_billboard)return(false);
        }

        return(true);
    }

    bool InitTexture()
    {
        TextureManager *tex_manager=GetTextureManager();

        texture=tex_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"),true);
        if(!texture)return(false);

        sampler=db->CreateSampler();

        if(!mi_billboard->GetMaterial()->BindImageSampler(  DescriptorSetType::PerMaterial,     ///<描述符合集
                                                            mtl::SamplerName::BaseColor,        ///<采样器名称
                                                            texture,                            ///<纹理
                                                            sampler))                           ///<采样器
            return(false);

        Vector2u texture_size(texture->GetWidth(),texture->GetHeight());

        mi_billboard->WriteMIData(texture_size);

        return(true);
    }

    SceneNode *CreateSceneNode(Primitive *r,MaterialInstance *mi,Pipeline *p)
    {
        Mesh *ri=db->CreateMesh(r,mi,p);

        if(!ri)
        {
            LOG_ERROR(OS_TEXT("Create Mesh failed."));
            return(nullptr);
        }

        return(new SceneNode(ri));
    }

    bool CreateRenderObject()
    {
        using namespace inline_geometry;

        {
            PrimitiveCreater pc(GetDevice(),mi_plane_grid->GetVIL());

            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.Set(500,500);
            pgci.sub_count.Set(5,5);

            pgci.lum=128;
            pgci.sub_lum=192;

            prim_plane_grid=CreatePlaneGrid2D(&pc,&pgci);
        }

        {
            PrimitiveCreater pc(GetDevice(),mi_billboard->GetVIL());

            pc.Init("Billboard",1);

            if(!pc.WriteVAB(VAN::Position,VF_V3F,position_data))
                return(false);

            ro_billboard=db->CreateMesh(&pc,mi_billboard,pipeline_billboard);

            if(!ro_billboard)
                return(false);
        }

        return(true);
    }

    bool InitScene()
    {
        SceneNode *scene_root=GetSceneRoot();       //取得缺省场景根节点
        Camera *cur_camera=GetCamera();             //取得缺省相机

        scene_root->Add(CreateSceneNode(prim_plane_grid,mi_plane_grid,pipeline_plane_grid));

        scene_root->Add(new SceneNode(ro_billboard));

        cur_camera->pos=Vector3f(32,32,32);

        CameraControl *camera_control=GetCameraControl();

        if(camera_control
         &&camera_control->GetControlName()==FirstPersonCameraControl::StaticControlName())
        {
            FirstPersonCameraControl *fp_cam_ctl=(FirstPersonCameraControl *)camera_control;

            fp_cam_ctl->SetTarget(Vector3f(0,0,0));
        }

        return(true);
    }

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(prim_plane_grid);
    }

    bool Init()
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
    return RunFramework<TestApp>(OS_TEXT("AutoInstance"),1024,1024);
}
