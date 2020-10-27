#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/InlineGeometry.h>
#include"VulkanAppFramework.h"

using namespace hgl;
using namespace hgl::graph;

constexpr uint OFFSCREEN_SIZE   =16;
constexpr uint SCREEN_WIDTH     =1024;
constexpr uint SCREEN_HEIGHT    =(SCREEN_WIDTH/16)*9;

class TestApp:public CameraAppFramework
{
    struct RenderObject
    {
        Camera cam;

        MaterialInstance *  material_instance   =nullptr;
        GPUBuffer *         ubo_world_matrix    =nullptr;
    };

    struct:public RenderObject
    {        
        RenderTarget *      render_taget        =nullptr;
        
        Pipeline *          pipeline            =nullptr;
        RenderableInstance *renderable_instance =nullptr;
    }os;
    
    struct:public RenderObject
    {
        Sampler *           sampler             =nullptr;
       
        Pipeline *          pipeline            =nullptr;
        RenderableInstance *renderable_instance =nullptr;

        SceneNode           scene_root;
        RenderList          render_list;
    }cube;

public:

    ~TestApp()
    {
        delete os.render_taget;
    }

    bool InitUBO(RenderObject *ro,const VkExtent2D &extent)
    {
        ro->cam.width=extent.width;
        ro->cam.height=extent.height;

        ro->cam.Refresh();

        ro->ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&ro->cam.matrix);

        if(!ro->ubo_world_matrix)
            return(false);
            
        ro->material_instance->BindUBO("world",ro->ubo_world_matrix);
        ro->material_instance->Update();
        return(true);
    }

    bool InitOffscreen()
    {
        os.render_taget=device->CreateColorRenderTarget(OFFSCREEN_SIZE,OFFSCREEN_SIZE,UFMT_RGBA8);
        if(!os.render_taget)return(false);
        
        os.material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/VertexColor2D"));
        if(!os.material_instance)return(false);

        os.pipeline=os.render_taget->CreatePipeline(os.material_instance,InlinePipeline::Solid2D,Prim::Fan);
        if(!os.pipeline)return(false);

        if(!InitUBO(&os,os.render_taget->GetExtent()))
            return(false);

        {
            CircleCreateInfo cci;
            
            cci.center.Set(OFFSCREEN_SIZE*0.5,OFFSCREEN_SIZE*0.5);
            cci.radius.Set(OFFSCREEN_SIZE*0.45,OFFSCREEN_SIZE*0.45);
            cci.field_count=32;
            cci.has_color=true;
            cci.center_color.Set(1,1,1,1);
            cci.border_color.Set(1,1,1,0);

            Renderable *render_obj=CreateRenderableCircle(db,os.material_instance->GetMaterial(),&cci);
            if(!render_obj)return(false);

            os.renderable_instance=db->CreateRenderableInstance(render_obj,os.material_instance,os.pipeline);

            if(!os.renderable_instance)return(false);
        }

        VulkanApplicationFramework::BuildCommandBuffer(os.render_taget,os.renderable_instance);

        os.render_taget->Submit(nullptr);
        os.render_taget->WaitQueue();

        return(true);
    }

    bool InitCube()
    {
        cube.material_instance=db->CreateMaterialInstance(OS_TEXT("res/material/TextureMask3D"));        
        if(!cube.material_instance)return(false);

        cube.pipeline=CreatePipeline(cube.material_instance,InlinePipeline::Solid3D);
        if(!cube.pipeline)return(false);
        
        cube.sampler=db->CreateSampler();
        if(!cube.sampler)return(false);

        cube.material_instance->BindSampler("tex",os.render_taget->GetColorTexture(),cube.sampler);
        cube.material_instance->BindUBO("world",GetCameraMatrixBuffer());
        cube.material_instance->Update();

        {
            CubeCreateInfo cci;

            Renderable *render_obj=CreateRenderableCube(db,cube.material_instance->GetMaterial(),&cci);
            if(!render_obj)return(false);

            cube.renderable_instance=db->CreateRenderableInstance(render_obj,cube.material_instance,cube.pipeline);

            cube.scene_root.Add(cube.renderable_instance);
        }
        
        cube.scene_root.RefreshMatrix();
        cube.scene_root.ExpendToList(&cube.render_list);

        camera.eye.Set(5,5,5,1.0);

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
        VulkanApplicationFramework::BuildCommandBuffer(index,&cube.render_list);
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
