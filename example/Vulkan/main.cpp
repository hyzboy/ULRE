#include"Window.h"
#include"VKInstance.h"
#include"VKPhysicalDevice.h"
#include"VKDevice.h"
#include"VKBuffer.h"
#include"VKShaderModule.h"
#include"VKShaderModuleManage.h"
#include"VKImageView.h"
#include"VKRenderable.h"
#include"VKDescriptorSets.h"
#include"VKRenderPass.h"
#include"VKPipeline.h"
#include"VKCommandBuffer.h"
#include"VKFormat.h"
#include"VKFramebuffer.h"
#include"VKMaterial.h"
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH=1280;
constexpr uint32_t SCREEN_HEIGHT=720;

VkShaderModule vs=nullptr;
VkShaderModule fs=nullptr;

struct WorldConfig
{
    Matrix4f mvp;    
}world;

vulkan::Buffer *CreateUBO(vulkan::Device *dev)
{
    const VkExtent2D extent=dev->GetExtent();

    world.mvp=ortho(extent.width,extent.height);

    return dev->CreateUBO(sizeof(WorldConfig),&world);
}

constexpr uint32_t VERTEX_COUNT=3;

constexpr float vertex_data[VERTEX_COUNT][2]=
{
    {SCREEN_WIDTH*0.5,   SCREEN_HEIGHT*0.25},
    {SCREEN_WIDTH*0.75,  SCREEN_HEIGHT*0.75},
    {SCREEN_WIDTH*0.25,  SCREEN_HEIGHT*0.75}
};

constexpr float color_data[VERTEX_COUNT][3]=
{   {1,0,0},
    {0,1,0},
    {0,0,1}
};

vulkan::VertexBuffer *vertex_buffer=nullptr;
vulkan::VertexBuffer *color_buffer=nullptr;

void CreateVertexBuffer(vulkan::Device *dev,vulkan::Renderable *render_obj)
{    
    vertex_buffer   =dev->CreateVBO(FMT_RG32F,  VERTEX_COUNT,vertex_data);
    color_buffer    =dev->CreateVBO(FMT_RGB32F, VERTEX_COUNT,color_data);

    render_obj->Set("Vertex",   vertex_buffer);
    render_obj->Set("Color",    color_buffer);
}

void wait_seconds(int seconds) {
#ifdef WIN32
    Sleep(seconds * 1000);
#elif defined(__ANDROID__)
    sleep(seconds);
#else
    sleep(seconds);
#endif
}

//class ExampleFramework
//{
//    Window *win=nullptr;
//    vulkan::Instance *inst=nullptr;
//    vulkan::Device *device=nullptr;
//    vulkan::Shader *shader=nullptr;
//    vulkan::Buffer *ubo_mvp=nullptr;
//    vulkan::VertexInput *vi=nullptr;
//    vulkan::PipelineCreater
//};//

int main(int,char **)
{
    #ifdef _DEBUG
    if(!vulkan::CheckStrideBytesByFormat())
        return 0xff;
    #endif//

    Window *win=CreateRenderWindow(OS_TEXT("VulkanTest"));

    win->Create(SCREEN_WIDTH,SCREEN_HEIGHT);

    vulkan::Instance *inst=vulkan::CreateInstance(U8_TEXT("VulkanTest"));

    if(!inst)
    {
        delete win;
        return(-1);
    }

    vulkan::Device *device=inst->CreateRenderDevice(win);

    if(!device)
    {
        delete inst;
        delete win;
        return(-2);
    }

    {
        const vulkan::PhysicalDevice *render_device=device->GetPhysicalDevice();

        std::cout<<"auto select physical device: "<<render_device->GetDeviceName()<<std::endl;
    }

    vulkan::ShaderModuleManage *shader_manage=device->CreateShaderModuleManage();

    const vulkan::Material *material=shader_manage->CreateMaterial("FlatColor.vert.spv","FlatColor.frag.spv");

    if(!material)
        return -3;

    vulkan::MaterialInstance *mi=material->CreateInstance();
    vulkan::Renderable *render_obj=mi->CreateRenderable();
    CreateVertexBuffer(device,render_obj);

    vulkan::Buffer *ubo=CreateUBO(device);
    mi->UpdateUBO("world",*ubo);

    vulkan::PipelineCreater pc(device,mi);    
    pc.SetDepthTest(false);
    pc.SetDepthWrite(false);
    pc.CloseCullFace();
    pc.Set(PRIM_TRIANGLES);

    vulkan::Pipeline *pipeline=pc.Create();

    if(!pipeline)
        return(-4);

    device->AcquireNextImage();

    vulkan::CommandBuffer *cmd_buf=device->CreateCommandBuffer();

    cmd_buf->Begin(device->GetRenderPass(),device->GetFramebuffer(0));
    cmd_buf->Bind(pipeline);
    cmd_buf->Bind(render_obj);
    cmd_buf->Draw(VERTEX_COUNT);
    cmd_buf->End();

    device->QueueSubmit(cmd_buf);
    device->Wait();
    device->QueuePresent();

    wait_seconds(1);

    delete vertex_buffer;
    delete color_buffer;

    delete pipeline;

    delete ubo;
    delete mi;

    delete cmd_buf;
    delete device;
    delete inst;
    delete win;

    return 0;
}
