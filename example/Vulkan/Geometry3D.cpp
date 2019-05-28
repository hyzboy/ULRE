﻿// 4.Geometry3D

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/SceneDB.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/RenderList.h>

using namespace hgl;
using namespace hgl::graph;

bool SaveToFile(const OSString &filename,VK_NAMESPACE::PipelineCreater *pc);
bool LoadFromFile(const OSString &filename,VK_NAMESPACE::PipelineCreater *pc);

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

class TestApp:public VulkanApplicationFramework
{
private:

    uint swap_chain_count=0;

    SceneDB *   db                  =nullptr;
    SceneNode   render_root;
    RenderList  render_list;

    Camera      camera;

    vulkan::Material *          material            =nullptr;
    vulkan::DescriptorSets *    descriptor_sets     =nullptr;

    vulkan::Renderable          *ro_plane_grid      =nullptr,
                                *ro_cube            =nullptr;

    vulkan::Buffer *            ubo_mvp             =nullptr;

    vulkan::Pipeline *          pipeline_line       =nullptr;
    vulkan::Pipeline *          pipeline_triangles  =nullptr;
    vulkan::CommandBuffer **    cmd_buf             =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(db);

        SAFE_CLEAR_OBJECT_ARRAY(cmd_buf,swap_chain_count);
    }

private:

    void InitCamera()
    {
        camera.type=CameraType::Perspective;
        camera.center.Set(0,0,0);
        camera.eye.Set(100,100,100);
        camera.up_vector.Set(0,0,1);
        camera.forward_vector.Set(0,1,0);
        camera.znear=4;
        camera.zfar=1000;
        camera.fov=90;
        camera.width=SCREEN_WIDTH;
        camera.height=SCREEN_HEIGHT;

        camera.Refresh();      //更新矩阵计算
    }

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("OnlyPosition3D.vert.spv"),
                                               OS_TEXT("FlatColor.frag.spv"));
        if(!material)
            return(false);

        descriptor_sets=material->CreateDescriptorSets();

        db->Add(material);
        db->Add(descriptor_sets);
        return(true);
    }

    void CreateRenderObject()
    {
        {
            struct PlaneGridCreateInfo pgci;

            pgci.coord[0].Set(-100,-100,0);
            pgci.coord[1].Set( 100,-100,0);
            pgci.coord[2].Set( 100, 100,0);
            pgci.coord[3].Set(-100, 100,0);

            pgci.step.u=20;
            pgci.step.v=20;

            ro_plane_grid=CreatePlaneGrid(db,material,&pgci);
        }

        {
            struct CubeCreateInfo cci;

            cci.tile.x=0;
            cci.tile.y=1;

            ro_cube=CreateCube(db,material,&cci);
        }
    }

    //bool InitUBO()
    //{
    //    const VkExtent2D extent=device->GetExtent();

    //    ubo_mvp=db->CreateUBO(sizeof(WorldConfig),&world);

    //    if(!ubo_mvp)
    //        return(false);

    //    if(!descriptor_sets->BindUBO(material->GetUBO("world"),*ubo_mvp))
    //        return(false);

    //    descriptor_sets->Update();
    //    return(true);
    //}

    bool InitPipeline()
    {
        constexpr os_char PIPELINE_FILENAME[]=OS_TEXT("2DSolid.pipeline");

        {
            vulkan::PipelineCreater *pipeline_creater=new vulkan::PipelineCreater(device,material,device->GetRenderPass(),device->GetExtent());
            pipeline_creater->SetDepthTest(true);
            pipeline_creater->SetDepthWrite(true);
            pipeline_creater->CloseCullFace();
            pipeline_creater->Set(PRIM_LINES);

            pipeline_line=pipeline_creater->Create();
            if(!pipeline_line)
                return(false);

            db->Add(pipeline_line);

            pipeline_creater->Set(PRIM_TRIANGLES);

            pipeline_triangles=pipeline_creater->Create();
            if(!pipeline_triangles)
                return(false);

            db->Add(pipeline_triangles);

            delete pipeline_creater;
        }

        return pipeline_line;
    }

    bool InitScene()
    {
        render_root.Add(db->CreateRenderableInstance(pipeline_line,descriptor_sets,ro_plane_grid));
        
        render_root.Add(db->CreateRenderableInstance(pipeline_triangles,descriptor_sets,ro_cube),scale(50,50,50));
        //render_root.Add(db->CreateRenderableInstance(pipeline,descriptor_sets,ro_circle));

        render_root.RefreshMatrix(&(camera.mvp));
        render_root.ExpendToList(&render_list);

        return(true);
    }

    bool InitCommandBuffer()
    {
        cmd_buf=hgl_zero_new<vulkan::CommandBuffer *>(swap_chain_count);

        for(uint i=0;i<swap_chain_count;i++)
        {
            cmd_buf[i]=device->CreateCommandBuffer();

            if(!cmd_buf[i])
                return(false);

            cmd_buf[i]->Begin();
            cmd_buf[i]->BeginRenderPass(device->GetRenderPass(),device->GetFramebuffer(i));
            render_list.Render(cmd_buf[i]);
            cmd_buf[i]->EndRenderPass();
            cmd_buf[i]->End();
        }

        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        swap_chain_count=device->GetSwapChainImageCount();

        db=new SceneDB(device);

        InitCamera();

        if(!InitMaterial())
            return(false);

        CreateRenderObject();

//        if(!InitUBO())
//            return(false);

        if(!InitPipeline())
            return(false);

        if(!InitScene())
            return(false);

        if(!InitCommandBuffer())
            return(false);

        return(true);
    }

    void Draw() override
    {
        const uint32_t frame_index=device->GetCurrentFrameIndices();

        const vulkan::CommandBuffer *cb=cmd_buf[frame_index];

        Submit(*cb);
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
