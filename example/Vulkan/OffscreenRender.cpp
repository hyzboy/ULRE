#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/InlineGeometry.h>
#include"VulkanAppFramework.h"

using namespace hgl;
using namespace hgl::graph;

constexpr uint OFFSCREEN_SIZE   =256;
constexpr uint SCREEN_WIDTH     =1024;
constexpr uint SCREEN_HEIGHT    =(SCREEN_WIDTH/16)*9;

class TestApp:public CameraAppFramework
{
    struct RenderObject
    {
        Camera cam;

        MaterialInstance *  material_instance   =nullptr;
        GPUBuffer *         ubo_camera_info     =nullptr;
    };

    struct:public RenderObject
    {        
        RenderTarget *      render_taget        =nullptr;
        RenderCmdBuffer *   command_buffer      =nullptr;
        
        Pipeline *          pipeline            =nullptr;
        RenderableInstance *renderable_instance =nullptr;

    public:

        bool Submit()
        {            
            if(!render_taget->Submit(command_buffer))
                return(false);
            if(!render_taget->WaitQueue())
                return(false);

            return(true);
        }
    }os;
    
    struct:public RenderObject
    {
        Sampler *           sampler             =nullptr;
       
        Pipeline *          pipeline            =nullptr;
        RenderableInstance *renderable_instance =nullptr;

        SceneNode           scene_root;
        RenderList          *render_list        =nullptr;
    }cube;

public:

    ~TestApp()
    {
        SAFE_CLEAR(cube.render_list);
        SAFE_CLEAR(os.render_taget);
    }

    bool InitUBO(RenderObject *ro,const VkExtent2D &extent)
    {
        ro->cam.width=extent.width;
        ro->cam.height=extent.height;

        ro->cam.Refresh();

        ro->ubo_camera_info=db->CreateUBO(sizeof(CameraInfo),&ro->cam.info);

        if(!ro->ubo_camera_info)
            return(false);
            
        {
            MaterialParameters *mp_global=ro->material_instance->GetMP(DescriptorSetType::Global);
        
            if(!mp_global)
                return(false);

            if(!mp_global->BindUBO("g_camera",ro->ubo_camera_info))return(false);

            mp_global->Update();
        }

        return(true);
    }

    bool InitOffscreen()
    {
        FramebufferInfo fbi(UPF_RGBA8,OFFSCREEN_SIZE,OFFSCREEN_SIZE);

        os.render_taget=device->CreateRenderTarget(&fbi);
        if(!os.render_taget)return(false);

        os.command_buffer=device->CreateRenderCommandBuffer();
        if(!os.command_buffer)return(false);
        
        os.material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor2D"));
        if(!os.material_instance)return(false);

        os.pipeline=os.render_taget->GetRenderPass()->CreatePipeline(os.material_instance,InlinePipeline::Solid2D,Prim::Fan);
        if(!os.pipeline)return(false);

        if(!InitUBO(&os,os.render_taget->GetExtent()))
            return(false);

        {
            CircleCreateInfo cci;
            
            cci.center=Vector2f(OFFSCREEN_SIZE*0.5,OFFSCREEN_SIZE*0.5);
            cci.radius=Vector2f(OFFSCREEN_SIZE*0.45,OFFSCREEN_SIZE*0.45);
            cci.field_count=32;
            cci.has_color=true;
            cci.center_color=Vector4f(1,1,1,1);
            cci.border_color=Vector4f(1,1,1,0);

            Renderable *render_obj=CreateRenderableCircle(db,os.material_instance->GetMaterial(),&cci);
            if(!render_obj)return(false);

            os.renderable_instance=db->CreateRenderableInstance(render_obj,os.material_instance,os.pipeline);

            if(!os.renderable_instance)return(false);
        }

        VulkanApplicationFramework::BuildCommandBuffer(os.command_buffer,os.render_taget,os.renderable_instance);

        os.Submit();

        return(true);
    }

    bool InitCube()
    {
        cube.render_list=new RenderList(device);

        cube.material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/TextureMask3D"));        
        if(!cube.material_instance)return(false);

        cube.pipeline=CreatePipeline(cube.material_instance,InlinePipeline::Solid3D,Prim::Triangles);
        if(!cube.pipeline)return(false);
        
        cube.sampler=db->CreateSampler();
        if(!cube.sampler)return(false);
        
        {
            MaterialParameters *mp_texture=cube.material_instance->GetMP(DescriptorSetType::Value);
        
            if(!mp_texture)
                return(false);
            
            if(!mp_texture->BindSampler("tex",os.render_taget->GetColorTexture(),cube.sampler))return(false);

            mp_texture->Update();
        }

        {
            CubeCreateInfo cci;

            Renderable *render_obj=CreateRenderableCube(db,cube.material_instance->GetMaterial(),&cci);
            if(!render_obj)return(false);

            cube.renderable_instance=db->CreateRenderableInstance(render_obj,cube.material_instance,cube.pipeline);

            cube.scene_root.CreateSubNode(cube.renderable_instance);
        }

        camera->pos=Vector4f(5,5,5,1.0);
        
        cube.scene_root.RefreshMatrix();
        cube.render_list->Expend(GetCameraInfo(),&cube.scene_root);

        return(true);
    }

    bool Init()
    {
        if(!CameraAppFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        SetClearColor(COLOR::MozillaCharcoal);

        if(!InitOffscreen())
            return(false);

        if(!InitCube())
            return(false);

        return(true);
    }

    void BuildCommandBuffer(uint32 index)
    {
        cube.scene_root.RefreshMatrix();
        cube.render_list->Expend(GetCameraInfo(),&cube.scene_root);

        VulkanApplicationFramework::BuildCommandBuffer(index,cube.render_list);
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
