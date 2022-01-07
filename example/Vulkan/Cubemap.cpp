// Cubemap
// CubeÃ˘Õº≤‚ ‘

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
constexpr uint32_t SCREEN_HEIGHT=720;

class TestApp:public CameraAppFramework
{
private:

    SceneNode           render_root;
    RenderList *        render_list         =nullptr;

    Material *          material            =nullptr;
    MaterialInstance *  material_instance   =nullptr;

    Pipeline *          pipeline_solid      =nullptr;

    GPUBuffer *         ubo_light           =nullptr;
    GPUBuffer *         ubo_phong           =nullptr;

    Sampler *           sampler             =nullptr;
    TextureCube *       texture             =nullptr;

    Renderable *        ro_cube             =nullptr;

private:

    bool InitMaterial()
    {
        {
            texture   =db->LoadTextureCube(OS_TEXT("res/cubemap/Test/Test.TexCube"),false);

            if(!texture)
                return(false);

            VkSamplerCreateInfo sampler_create_info=
            {
                VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                nullptr,
                0,
                VK_FILTER_NEAREST,
                VK_FILTER_NEAREST,
                VK_SAMPLER_MIPMAP_MODE_NEAREST,
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
            material=db->CreateMaterial(OS_TEXT("res/material/Skybox"));
            if(!material)return(false);

            material_instance=db->CreateMaterialInstance(material);
            if(!material_instance)return(false);

            {            
                MaterialParameters *mp_texture=material_instance->GetMP(DescriptorSetsType::Value);

                if(!mp_texture)
                    return(false);

                if(!mp_texture->BindSampler("tex"    ,texture,    sampler))
                    return(false);

                mp_texture->Update();
            }
        }

        pipeline_solid=CreatePipeline(material_instance,InlinePipeline::Solid3D,Prim::Triangles);
        if(!pipeline_solid)return(false);

        return(true);
    }

    void CreateRenderObject()
    {
        const VAB *vab=material_instance->GetVAB();

        {
            struct CubeCreateInfo cci;

            ro_cube=CreateRenderableCube(db,vab,&cci);
        }
    }

    void Add(Renderable *r,Pipeline *pl)
    {
        auto ri=db->CreateRenderableInstance(r,material_instance,pl);

        render_root.CreateSubNode(ri);
    }

    void Add(Renderable *r,Pipeline *pl,const Matrix4f &mat)
    {
        auto ri=db->CreateRenderableInstance(r,material_instance,pl);

        render_root.CreateSubNode(mat,ri);
    }

    bool InitScene()
    {
        Add(ro_cube,pipeline_solid);

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

        render_list=new RenderList(device);

        if(!InitMaterial())
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
