// 5.SceneTree
//      用于测试树形排列的场景中，每一级节点对变换矩阵的处理是否正确

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/SceneDB.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/RenderList.h>
#include<hgl/Time.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=256;
constexpr uint32_t SCREEN_HEIGHT=256;

class TestApp:public VulkanApplicationFramework
{
private:

    double start_time;

    uint swap_chain_count=0;

    SceneDB *   db                  =nullptr;
    SceneNode   render_root;
    RenderList  render_list;

    Camera      camera;

    vulkan::Material *          material            =nullptr;
    vulkan::DescriptorSets *    descriptor_sets     =nullptr;

    vulkan::Renderable *        ro_cube             =nullptr;

    vulkan::Buffer *            ubo_world_matrix    =nullptr;

    vulkan::Pipeline *          pipeline_line       =nullptr;
    vulkan::CommandBuffer **    cmd_buf             =nullptr;

public:

    TestApp()
    {
        start_time=GetDoubleTime();
    }

    ~TestApp()
    {
        SAFE_CLEAR(db);

        SAFE_CLEAR_OBJECT_ARRAY(cmd_buf,swap_chain_count);
    }

private:

    void InitCamera()
    {
        camera.type=CameraType::Perspective;
        camera.center.Set(0,0,0,1);
        camera.eye.Set(100,100,100,1);
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
        struct CubeCreateInfo cci;

        ro_cube=CreateCube(db,material,&cci);
    }

    bool InitUBO()
    {
        const VkExtent2D extent=device->GetExtent();

        ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&camera.matrix);

        if(!ubo_world_matrix)
            return(false);

        if(!descriptor_sets->BindUBO(material->GetUBO("world"),*ubo_world_matrix))
            return(false);

        descriptor_sets->Update();
        return(true);
    }

    bool InitPipeline()
    {
        constexpr os_char PIPELINE_FILENAME[]=OS_TEXT("2DSolid.pipeline");

        {
            vulkan::PipelineCreater *pipeline_creater=new vulkan::PipelineCreater(device,material,device->GetRenderPass(),device->GetExtent());
            pipeline_creater->SetDepthTest(true);
            pipeline_creater->SetDepthWrite(true);
            pipeline_creater->CloseCullFace();
            pipeline_creater->SetPolygonMode(VK_POLYGON_MODE_LINE);
            pipeline_creater->Set(PRIM_TRIANGLES);

            pipeline_line=pipeline_creater->Create();
            if(!pipeline_line)
                return(false);

            db->Add(pipeline_line);

            delete pipeline_creater;
        }

        return pipeline_line;
    }

    bool InitScene()
    {
        SceneNode *cur_node=&render_root;

        uint size=1;
        uint x=0;

        const Vector3f axis(0,0,1);

        RenderableInstance *ri=db->CreateRenderableInstance(pipeline_line,descriptor_sets,ro_cube);

        for(uint i=0;i<360;i++)
        {
            render_root.Add(ri,
                            rotate(i/5.0f,axis)*
                            translate(i/4.0f,0,0)*
                            scale((i+1)/100.0f));
        }

        return(true);
    }

    bool InitCommandBuffer()
    {
        cmd_buf=hgl_zero_new<vulkan::CommandBuffer *>(swap_chain_count);

        for(uint i=0;i<swap_chain_count;i++)
            cmd_buf[i]=device->CreateCommandBuffer();

        return BuildCommandBuffer();
    }

    bool BuildCommandBuffer()
    {
        for(uint i=0;i<swap_chain_count;i++)
        {
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

        if(!InitUBO())
            return(false);

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
        vulkan::CommandBuffer *cb=cmd_buf[device->GetCurrentFrameIndices()];

        Submit(*cb);

        Matrix4f rot=rotate(GetDoubleTime()-start_time,camera.right_vector);

        render_root.RefreshMatrix(&rot);
        render_list.Clear();
        render_root.ExpendToList(&render_list);

        BuildCommandBuffer();
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
