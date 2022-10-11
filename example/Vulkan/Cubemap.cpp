// Cubemap
// Cube贴图测试

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

class TestApp:public CameraAppFramework
{
private:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

    Material *          sky_material        =nullptr;
    MaterialInstance *  sky_mi              =nullptr;

    Material *          axis_material       =nullptr;
    MaterialInstance *  axis_mi             =nullptr;

    Material *          envmap_material     =nullptr;
    MaterialInstance *  envmap_mi           =nullptr;

    Pipeline *          axis_pipeline       =nullptr;
    Pipeline *          sky_pipeline        =nullptr;
    Pipeline *          solid_pipeline      =nullptr;

    GPUBuffer *         ubo_light           =nullptr;
    GPUBuffer *         ubo_phong           =nullptr;

    Sampler *           sampler             =nullptr;
    TextureCube *       texture             =nullptr;

    Primitive *        ro_axis             =nullptr;
    Primitive *        ro_cube             =nullptr;
    Primitive *        ro_sphere           =nullptr;

private:

    bool InitMaterial()
    {
        {
            axis_material=db->CreateMaterial(OS_TEXT("res/material/VertexColor3D"));
            if(!axis_material)return(false);

            axis_mi=db->CreateMaterialInstance(axis_material);
            if(!axis_mi)return(false);

            axis_pipeline=CreatePipeline(axis_mi,InlinePipeline::Solid3D,Prim::Lines);
            if(!axis_pipeline)return(false);
        }

        {
            texture   =db->LoadTextureCube(OS_TEXT("res/cubemap/Storforsen4.TexCube"),false);

            if(!texture)
                return(false);

            VkSamplerCreateInfo sampler_create_info=
            {
                VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                nullptr,
                0,
                VK_FILTER_LINEAR,
                VK_FILTER_LINEAR,
                VK_SAMPLER_MIPMAP_MODE_LINEAR,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                0.0f,
                VK_FALSE,
                0,
                false,
                VK_COMPARE_OP_NEVER,
                0.0f,
                0,
                VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                false
            };

            sampler =db->CreateSampler(&sampler_create_info);
        }

        {
            sky_material=db->CreateMaterial(OS_TEXT("res/material/Skybox"));
            if(!sky_material)return(false);

            sky_mi=db->CreateMaterialInstance(sky_material);
            if(!sky_mi)return(false);

            if(!sky_mi->BindSampler(DescriptorSetsType::Value,"tex"    ,texture,    sampler))return(false);

            sky_pipeline=CreatePipeline(sky_mi,InlinePipeline::Sky,Prim::Triangles);
            if(!sky_pipeline)return(false);
        }

        {
            envmap_material=db->CreateMaterial(OS_TEXT("res/material/EnvCubemap"));
            if(!envmap_material)return(false);

            envmap_mi=db->CreateMaterialInstance(envmap_material);
            if(!envmap_mi)return(false);

            if(!envmap_mi->BindSampler(DescriptorSetsType::Value,"EnvCubemap"    ,texture,    sampler))return(false);

            solid_pipeline=CreatePipeline(envmap_mi,InlinePipeline::Solid3D,Prim::Triangles);
        }

        return(true);
    }

    void CreateRenderObject()
    {
        using namespace inline_geometry;

        {
            struct AxisCreateInfo aci;

            aci.size=GetCameraInfo().zfar;

            ro_axis=CreateAxis(db,axis_mi->GetVIL(),&aci);
        }
        
        {
            struct CubeCreateInfo cci;

            ro_cube=CreateCube(db,sky_mi->GetVIL(),&cci);
        }

        {
            ro_sphere=CreateSphere(db,envmap_mi->GetVIL(),64);
        }
    }

    bool InitUBO()
    {
        if(!BindCameraUBO(sky_mi))return(false);
        if(!BindCameraUBO(envmap_mi))return(false);

        return(true);
    }

    SceneNode *Add(Primitive *r,MaterialInstance *mi,Pipeline *pl)
    {
        auto ri=db->CreateRenderable(r,mi,pl);

        return render_root.CreateSubNode(ri);
    }

    bool InitScene()
    {
        Add(ro_axis,axis_mi,axis_pipeline);
        Add(ro_cube,sky_mi,sky_pipeline);
        Add(ro_sphere,envmap_mi,solid_pipeline)->SetLocalMatrix(scale(5,5,5));

        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(),&render_root);

        return(true);
    }

public:

    ~TestApp()
    {
        SAFE_CLEAR(render_list);
    }

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        camera->pos=Vector3f(10,10,0);

        camera_control->SetTarget(Vector3f(0,0,0));

        //camera->Refresh();      //更新矩阵计算

        render_list=new RenderList(device);

        if(!InitMaterial())
            return(false);

        if(!InitUBO())
            return(false);

        CreateRenderObject();

        if(!InitScene())
            return(false);

        return(true);
    }

    void BuildCommandBuffer(uint32_t index) override
    {
        render_root.RefreshMatrix();
        render_list->Expend(GetCameraInfo(),&render_root);

        VulkanApplicationFramework::BuildCommandBuffer(index,render_list);
    }
};//class TestApp:public CameraAppFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
