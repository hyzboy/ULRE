// Equirectangular ( or spherical)
// 等距矩形（或球形）贴图反射测试

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=512;

class TestApp:public CameraAppFramework
{
private:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

    Material *          envmap_material     =nullptr;
    MaterialInstance *  envmap_mi           =nullptr;

    Pipeline *          solid_pipeline      =nullptr;

    GPUBuffer *         ubo_light           =nullptr;
    GPUBuffer *         ubo_phong           =nullptr;

    Sampler *           sampler             =nullptr;
    Texture2D *         texture             =nullptr;

    Renderable *        ro_sphere           =nullptr;

    SceneNode *         sn_sphere           =nullptr;

    double              last_time;

private:

    bool InitMaterial()
    {
        {
            // photo source: https://www.hqreslib.com
            // license: CY-BY

            texture   =db->LoadTexture2D(OS_TEXT("res/equirectangular/jifu.Tex2D"),false);

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
            envmap_material=db->CreateMaterial(OS_TEXT("res/material/EnvEquirectangularMap"));
            if(!envmap_material)return(false);

            envmap_mi=db->CreateMaterialInstance(envmap_material);
            if(!envmap_mi)return(false);

            {
                MaterialParameters *mp_texture=envmap_mi->GetMP(DescriptorSetsType::Value);

                if(!mp_texture)
                    return(false);

                if(!mp_texture->BindSampler("Envmap"    ,texture,    sampler))
                    return(false);

                mp_texture->Update();
            }

            solid_pipeline=CreatePipeline(envmap_mi,InlinePipeline::Solid3D,Prim::Triangles);
        }

        return(true);
    }

    void CreateRenderObject()
    {
        ro_sphere=CreateRenderableSphere(db,envmap_mi->GetVAB(),64);
    }

    bool InitUBO()
    {
        if(!BindCameraUBO(envmap_mi))return(false);

        return(true);
    }

    SceneNode *Add(Renderable *r,MaterialInstance *mi,Pipeline *pl)
    {
        auto ri=db->CreateRenderableInstance(r,mi,pl);

        return render_root.CreateSubNode(ri);
    }

    bool InitScene()
    {
        sn_sphere=Add(ro_sphere,envmap_mi,solid_pipeline);
        sn_sphere->SetLocalMatrix(scale(5,5,5));

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

        last_time=GetDoubleTime();

        camera->pos=Vector3f(10,10,0);

        camera_control->SetTarget(Vector3f(0,0,0));

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
