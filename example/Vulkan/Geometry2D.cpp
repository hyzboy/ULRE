// 3.Geometry2D
// 该范例有两个作用：
//                  一、测试绘制2D几何体
//                  二、试验动态合并材质渲染机制、包括普通合并与Instance

#include"VulkanAppFramework.h"
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/VertexBuffer.h>

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

struct RectangleCreateInfo
{
    RectScope2f scope;
};

VK_NAMESPACE_BEGIN
/**
 * 场景DB，用于管理场景内所需的所有数据
 */
class SceneDatabase
{
    Set<Material *>         material_sets;                              ///<材质合集
    Set<Pipeline *>         pipeline_sets;                              ///<管线合集
    Set<DescriptorSets *>   desc_sets_sets;                             ///<描述符合集
    Set<Renderable *>       renderable_sets;                            ///<可渲染对象合集
};//class SceneDatabase

/**
 * 可渲染对象实例
 */
class RenderableInstance
{
    Pipeline *        pipeline;
    DescriptorSets *  desc_sets;
    Renderable *      render_obj;

public:

    RenderableInstance(Pipeline *p,DescriptorSets *ds,Renderable *r):pipeline(p),desc_sets(ds),render_obj(r){}
    virtual ~RenderableInstance()=default;

    Pipeline *      GetPipeline         (){return pipeline;}
    DescriptorSets *GetDescriptorSets   (){return desc_sets;}
    Renderable *    GetRenderable       (){return render_obj;}

    const int Comp(const RenderableInstance *ri)const
    {
        //绘制顺序：
        
        //  ARM Mali GPU :   不透明、AlphaTest、半透明
        //  Adreno/NV/AMD:   AlphaTest、不透明、半透明
        //  PowerVR:         同Adreno/NV/AMD，但不透明部分可以不排序

        if(pipeline->IsAlphaTest())
        {
            if(!ri->pipeline->IsAlphaTest())
                return -1;
        }
        else
        {
            if(ri->pipeline->IsAlphaTest())
                return 1;
        }

        if(pipeline->IsAlphaBlend())
        {
            if(!ri->pipeline->IsAlphaBlend())
                return 1;
        }
        else
        {
            if(ri->pipeline->IsAlphaBlend())
                return -1;
        }

        if(pipeline!=ri->pipeline)
            return pipeline-ri->pipeline;

        if(desc_sets!=ri->desc_sets)
            return desc_sets-ri->desc_sets;

        return render_obj-ri->render_obj;
    }

    CompOperator(const RenderableInstance *,Comp)
};//class RenderableInstance
VK_NAMESPACE_END

constexpr uint32_t VERTEX_COUNT=4;

vulkan::Renderable *CreateRectangle(vulkan::Device *device,vulkan::Material *mtl,const RectangleCreateInfo *rci)
{
    const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

    vulkan::Renderable *render_obj=mtl->CreateRenderable(VERTEX_COUNT);

    const int vertex_binding=vsm->GetStageInputBinding("Vertex");
    
    if(vertex_binding!=-1)
    {
        VB2f *vertex=new VB2f(VERTEX_COUNT);

        vertex->Begin();
            vertex->WriteRectFan(rci->scope);
        vertex->End();

        render_obj->Set(vertex_binding,device->CreateVBO(vertex));
        delete vertex;
    }
    else
    {
        delete render_obj;
        return nullptr;
    }

    return render_obj;
}

struct RoundRectangleCreateInfo:public RectangleCreateInfo
{
    float radius;           //圆角半径
    uint32_t round_per;     //圆角精度
};

vulkan::Renderable *CreateRoundRectangle(vulkan::Device *device,vulkan::Material *mtl,const RoundRectangleCreateInfo *rci)
{
    const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

    vulkan::Renderable *render_obj=nullptr;
    const int vertex_binding=vsm->GetStageInputBinding("Vertex");

    if(vertex_binding==-1)
        return(nullptr);
    
    VB2f *vertex=nullptr;

    if(rci->radius==0||rci->round_per<=1)      //这是要画矩形
    {        
        vertex=new VB2f(4);

        vertex->Begin();
        vertex->WriteRectFan(rci->scope);
        vertex->End();
    }
    else
    {
        float radius=rci->radius;

        if(radius>rci->scope.GetWidth()/2.0f)radius=rci->scope.GetWidth()/2.0f;
        if(radius>rci->scope.GetHeight()/2.0f)radius=rci->scope.GetHeight()/2.0f;

        vertex=new VB2f(1+rci->round_per*4);

        vertex->Begin();

            vec2<float> *coord=new vec2<float>[rci->round_per];

            for(uint i=0;i<rci->round_per;i++)
            {
                float ang=float(i)/float(rci->round_per-1)*90.0f;

                float x=sin(hgl_ang2rad(ang))*radius;
                float y=cos(hgl_ang2rad(ang))*radius;

                coord[i].x=x;
                coord[i].y=y;

                //生成的是右上角的
                vertex->Write(  rci->scope.GetRight()-radius+x,
                                rci->scope.GetTop()+radius-y);
            }

            //右下角
            for(uint i=0;i<rci->round_per;i++)
            {
                vertex->Write(rci->scope.GetRight() -radius+coord[rci->round_per-1-i].x,
                              rci->scope.GetBottom()-radius+coord[rci->round_per-1-i].y);
            }

            //左下角
            for(uint i=0;i<rci->round_per;i++)
            {
                vertex->Write(rci->scope.GetLeft()  +radius-coord[i].x,
                              rci->scope.GetBottom()-radius+coord[i].y);
            }

            //左上角
            for(uint i=0;i<rci->round_per;i++)
            {
                vertex->Write(rci->scope.GetLeft()  +radius-coord[rci->round_per-1-i].x,
                              rci->scope.GetTop()   +radius-coord[rci->round_per-1-i].y);
            }

            vertex->Write(rci->scope.GetRight() -radius+coord[0].x,
                          rci->scope.GetTop()   +radius-coord[0].y);

            delete[] coord;

        vertex->End();
    }

    render_obj=mtl->CreateRenderable(vertex->GetCount());
    render_obj->Set(vertex_binding,device->CreateVBO(vertex));

    delete vertex;

    return render_obj;
}

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

        struct RoundRectangleCreateInfo rrci;

        rrci.scope.Set(10,10,SCREEN_WIDTH-20,SCREEN_HEIGHT-20);
        rrci.radius=8;     //半径为8
        rrci.round_per=8;   //圆角切分成8段

        render_obj=CreateRoundRectangle(device,material,&rrci);

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
