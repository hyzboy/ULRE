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


    Material *          axis_material       =nullptr;
    MaterialInstance *  axis_mi             =nullptr;

    Pipeline *          axis_pipeline       =nullptr;
    Pipeline *          pipeline_solid      =nullptr;

    GPUBuffer *         ubo_light           =nullptr;
    GPUBuffer *         ubo_phong           =nullptr;

    Sampler *           sampler             =nullptr;
    TextureCube *       texture             =nullptr;

    Renderable *        ro_axis             =nullptr;
    Renderable *        ro_cube             =nullptr;

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
        {
            struct AxisCreateInfo aci;

            aci.size=200;

            ro_axis=CreateRenderableAxis(db,axis_mi->GetVAB(),&aci);
        }

        const VAB *vab=material_instance->GetVAB();

        {
            struct CubeCreateInfo cci;

            ro_cube=CreateRenderableCube(db,vab,&cci);
        }
    }

    bool InitUBO()
    {
        {
            MaterialParameters *mp_global=axis_mi->GetMP(DescriptorSetsType::Global);

            if(!mp_global)
                return(false);

            if(!mp_global->BindUBO("g_camera",GetCameraInfoBuffer()))return(false);

            mp_global->Update();
        }

        return(true);
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
        render_root.CreateSubNode(db->CreateRenderableInstance(ro_axis,axis_mi,axis_pipeline));

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
