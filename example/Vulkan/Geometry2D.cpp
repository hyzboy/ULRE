﻿// 3.Geometry2D
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
    vec3<float> normal;
    Color4f color;
    RectScope2f tex_coord;
};

vulkan::Renderable *CreateRectangle(vulkan::Device *device,vulkan::Material *mtl,const RectangleCreateInfo *rci)
{
    const vulkan::VertexShaderModule *vsm=mtl->GetVertexShaderModule();

    vulkan::Renderable *render_obj=mtl->CreateRenderable();

    const int vertex_binding=vsm->GetStageInputBinding("Vertex");
    
    if(vertex_binding!=-1)
    {
        VB2f *vertex=new VB2f(6);

        vertex->Begin();
            vertex->WriteRect(rci->scope);
        vertex->End();

        render_obj->Set(vertex_binding,device->CreateVBO(vertex));
        delete vertex;
    }

    const int normal_binding=vsm->GetStageInputBinding("Normal");

    if(normal_binding!=-1)
    {
        VB3f *normal=new VB3f(6);

        normal->Begin();
            normal->Write(rci->normal,6);
        normal->End();

        render_obj->Set(normal_binding,device->CreateVBO(normal));
        delete normal;
    }

    const int color_binding=vsm->GetStageInputBinding("Color");
    
    if(color_binding!=-1)
    {
        VB4f *color=new VB4f(6);

        color->Begin();
            color->Write(rci->color,6);
        color->End();

        render_obj->Set(color_binding,device->CreateVBO(color));
        delete color;
    }

    const int tex_coord_binding=vsm->GetStageInputBinding("TexCoord");
    
    if(tex_coord_binding!=-1)
    {
        VB2f *tex_coord=new VB2f(6);

        tex_coord->Begin();
            tex_coord->WriteRect(rci->tex_coord);
        tex_coord->End();

        render_obj->Set(tex_coord_binding,device->CreateVBO(tex_coord));
        delete tex_coord;
    }

    return render_obj;
}

/**
 * 创建一个圆角矩形
 * @param r 半径
 * @param rp 半径精度
 */
//Mesh *CreateRoundrectangle(float l,float t,float w,float h,float r,uint32_t rp)
//{
//}

class TestApp:public VulkanApplicationFramework
{
private:

    uint swap_chain_count=0;

    vulkan::Material *          material            =nullptr;
    vulkan::DescriptorSets *    desciptor_sets      =nullptr;
    vulkan::Renderable *        render_obj          =nullptr;
    vulkan::Buffer *            ubo_mvp             =nullptr;

    vulkan::Pipeline *          pipeline            =nullptr;
    vulkan::CommandBuffer **    cmd_buf             =nullptr;

    vulkan::VertexBuffer *      vertex_buffer       =nullptr;
    vulkan::VertexBuffer *      color_buffer        =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(color_buffer);
        SAFE_CLEAR(vertex_buffer);
        SAFE_CLEAR_OBJECT_ARRAY(cmd_buf,swap_chain_count);
        SAFE_CLEAR(pipeline);
        SAFE_CLEAR(ubo_mvp);
        SAFE_CLEAR(render_obj);
        SAFE_CLEAR(desciptor_sets);
        SAFE_CLEAR(material);
    }

private:

    bool InitMaterial()
    {
        material=shader_manage->CreateMaterial(OS_TEXT("OnlyPosition.vert.spv"),
                                               OS_TEXT("FlatColor.frag.spv"));
        if(!material)
            return(false);

        desciptor_sets=material->CreateDescriptorSets();
        return(true);
    }

    bool CreateRenderObject()
    {
        struct RectangleCreateInfo rci;

        rci.scope.Set(10,10,10,10);

        render_obj=CreateRectangle(device,material,&rci);

        return render_obj;
    }

    bool InitUBO()
    {
        const VkExtent2D extent=device->GetExtent();

        world.mvp=ortho(extent.width,extent.height);

        ubo_mvp=device->CreateUBO(sizeof(WorldConfig),&world);

        if(!ubo_mvp)
            return(false);

        if(!desciptor_sets->BindUBO(material->GetUBO("world"),*ubo_mvp))
            return(false);

        desciptor_sets->Update();
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
            pipeline_creater->Set(PRIM_TRIANGLES);

            pipeline=pipeline_creater->Create();

            delete pipeline_creater;
        }

        return pipeline;
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
                    cmd_buf[i]->Bind(pipeline);
                    cmd_buf[i]->Bind(desciptor_sets);
                    cmd_buf[i]->Bind(render_obj);
                    cmd_buf[i]->Draw(6);
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
