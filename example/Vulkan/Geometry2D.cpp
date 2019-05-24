// 3.Geometry2D
// 该范例有两个作用：
//                  一、测试绘制2D几何体
//                  二、试验动态合并材质渲染机制、包括普通合并与Instance

#include"VulkanAppFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/type/ResManage.h>

using namespace hgl;
using namespace hgl::graph;

bool SaveToFile(const OSString &filename,VK_NAMESPACE::PipelineCreater *pc);
bool LoadFromFile(const OSString &filename,VK_NAMESPACE::PipelineCreater *pc);

constexpr uint32_t SCREEN_WIDTH=128;
constexpr uint32_t SCREEN_HEIGHT=128;

struct WorldConfig
{
    Matrix4f mvp;
}world;

VK_NAMESPACE_BEGIN
//using MaterialID=int;
//using PipelineID=int;
//using DescriptorSetsID=int;
//using RenderableID=int;
//
///**
// * 场景DB，用于管理场景内所需的所有数据
// */
//class SceneDatabase
//{
//    IDResManage<MaterialID,       Material>       rm_material;                                        ///<材质合集
//    IDResManage<PipelineID,       Pipeline>       rm_pipeline;                                        ///<管线合集
//    IDResManage<DescriptorSetsID, DescriptorSets> rm_desc_sets;                                       ///<描述符合集
//    IDResManage<RenderableID,     Renderable>     rm_renderable;                                      ///<可渲染对象合集
//
//public:
//
//    MaterialID          Add(Material *      mtl ){return rm_material.Add(mtl);}
//    PipelineID          Add(Pipeline *      p   ){return rm_pipeline.Add(p);}
//    DescriptorSetsID    Add(DescriptorSets *ds  ){return rm_desc_sets.Add(ds);}
//    RenderableID        Add(Renderable *    r   ){return rm_renderable.Add(r);}
//};//class SceneDatabase

VK_NAMESPACE_END

class TestApp:public VulkanApplicationFramework
{
private:

    uint swap_chain_count=0;

    vulkan::Material *          material            =nullptr;
    vulkan::DescriptorSets *    descriptor_sets     =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::RenderableInstance *render_instance     =nullptr;

    vulkan::Buffer *            ubo_mvp             =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;
    vulkan::CommandBuffer **    cmd_buf             =nullptr;

    vulkan::VertexBuffer *      vertex_buffer       =nullptr;
    vulkan::VertexBuffer *      color_buffer        =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(render_instance);
        SAFE_CLEAR(color_buffer);
        SAFE_CLEAR(vertex_buffer);
        SAFE_CLEAR_OBJECT_ARRAY(cmd_buf,swap_chain_count);
        SAFE_CLEAR(pipeline);
        SAFE_CLEAR(ubo_mvp);
        SAFE_CLEAR(render_obj);
        SAFE_CLEAR(descriptor_sets);
        SAFE_CLEAR(material);
    }

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("OnlyPosition.vert.spv"),
                                               OS_TEXT("FlatColor.frag.spv"));
        if(!material)
            return(false);

        descriptor_sets=material->CreateDescriptorSets();
        return(true);
    }

    bool CreateRenderObject()
    {
        //struct RectangleCreateInfo rci;

        //rci.scope.Set(10,10,10,10);

        //render_obj=CreateRectangle(device,material,&rci);

        //struct RoundRectangleCreateInfo rrci;

        //rrci.scope.Set(10,10,SCREEN_WIDTH-20,SCREEN_HEIGHT-20);
        //rrci.radius=8;     //半径为8
        //rrci.round_per=8;   //圆角切分成8段

        //render_obj=CreateRoundRectangle(device,material,&rrci);

        struct CircleCreateInfo cci;

        cci.center.x=SCREEN_WIDTH/2;
        cci.center.y=SCREEN_HEIGHT/2;

        cci.radius.x=SCREEN_WIDTH*0.45;
        cci.radius.y=SCREEN_HEIGHT*0.45;

        cci.field_count=8;

        render_obj=CreateCircle(device,material,&cci);

        return render_obj;
    }

    bool InitUBO()
    {
        const VkExtent2D extent=device->GetExtent();

        world.mvp=ortho(extent.width,extent.height);

        ubo_mvp=device->CreateUBO(sizeof(WorldConfig),&world);

        if(!ubo_mvp)
            return(false);

        if(!descriptor_sets->BindUBO(material->GetUBO("world"),*ubo_mvp))
            return(false);

        descriptor_sets->Update();
        return(true);
    }
    
    bool InitPipeline()
    {
        constexpr os_char PIPELINE_FILENAME[]=OS_TEXT("2DSolid.pipeline");

        {
            vulkan::PipelineCreater *pipeline_creater=new vulkan::PipelineCreater(device,material,device->GetRenderPass(),device->GetExtent());
            pipeline_creater->SetDepthTest(false);
            pipeline_creater->SetDepthWrite(false);
            pipeline_creater->CloseCullFace();
            pipeline_creater->Set(PRIM_TRIANGLE_FAN);

            pipeline=pipeline_creater->Create();

            delete pipeline_creater;
        }

        return pipeline;
    }

    void Draw(vulkan::CommandBuffer *cb,vulkan::RenderableInstance *ri)
    {
        cb->Bind(ri->GetPipeline());
        cb->Bind(ri->GetDescriptorSets());

        vulkan::Renderable *obj=ri->GetRenderable();

        cb->Bind(obj);

        const vulkan::IndexBuffer *ib=obj->GetIndexBuffer();

        if(ib)
        {
            cb->DrawIndexed(ib->GetCount());
        }
        else
        {
            cb->Draw(obj->GetDrawCount());
        }
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
                    Draw(cmd_buf[i],render_instance);
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

        if(!InitMaterial())
            return(false);

        if(!CreateRenderObject())
            return(false);

        if(!InitUBO())
            return(false);
            
        if(!InitPipeline())
            return(false);

        render_instance=new vulkan::RenderableInstance(pipeline,descriptor_sets,render_obj);

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
