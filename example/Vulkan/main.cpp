#include"Window.h"
#include"VKInstance.h"
#include"VKPhysicalDevice.h"
#include"VKDevice.h"
#include"VKBuffer.h"
#include"VKShader.h"
#include"VKImageView.h"
#include"VKVertexInput.h"
#include"VKDescriptorSets.h"
#include"VKRenderPass.h"
#include"VKPipelineLayout.h"
#include"VKPipeline.h"
#include"VKCommandBuffer.h"
#include"VKSemaphore.h"
#include"VKFormat.h"
#include"VKFramebuffer.h"

#include<fstream>
#ifdef WIN32
#else
#include<unistd.h>
#endif//

using namespace hgl;
using namespace hgl::graph;

VkShaderModule vs=nullptr;
VkShaderModule fs=nullptr;

char *LoadFile(const char *filename,uint32_t &file_length)
{
    std::ifstream fs;

    fs.open(filename,std::ios_base::binary);

    if(!fs.is_open())
        return(nullptr);

    fs.seekg(0,std::ios_base::end);
    file_length=fs.tellg();
    char *data=new char[file_length];

    fs.seekg(0,std::ios_base::beg);
    fs.read(data,file_length);

    fs.close();
    return data;
}

bool LoadShader(vulkan::Shader *sc,const char *filename,VkShaderStageFlagBits shader_flag)
{
    uint32_t size;
    char *data=LoadFile(filename,size);

    if(!data)
        return(false);

    if(!sc->Add(shader_flag,data,size))
        return(false);

    delete[] data;
    return(true);
}

vulkan::Shader *LoadShader(VkDevice device)
{
    vulkan::Shader *sc=new vulkan::Shader(device);

    if(LoadShader(sc,"FlatColor.vert.spv",VK_SHADER_STAGE_VERTEX_BIT))
    if(LoadShader(sc,"FlatColor.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT))
        return sc;

    delete sc;
    return(nullptr);
}

constexpr float vertex_data[]={0.0f,0.5f,   -0.5f,-0.5f,    0.5f,-0.5f };
constexpr float color_data[]={1,0,0,    0,1,0,      0,0,1   };

vulkan::VertexBuffer *vertex_buffer=nullptr;
vulkan::VertexBuffer *color_buffer=nullptr;

vulkan::VertexInput *CreateVertexBuffer(vulkan::Device *dev)
{
    vertex_buffer=dev->CreateVBO(FMT_RG32F,6*sizeof(float));
    color_buffer=dev->CreateVBO(FMT_RGB32F,9*sizeof(float));

    {
        float *p=(float *)vertex_buffer->Map();

        memcpy(p,vertex_data,6*sizeof(float));

        vertex_buffer->Unmap();
    }

    {
        float *p=(float *)color_buffer->Map();

        memcpy(p,color_data,9*sizeof(float));

        color_buffer->Unmap();
    }

    vulkan::VertexInput *vi=new vulkan::VertexInput();

    constexpr uint32_t position_shader_location=0;          //对应shader中的layout(locaiton=0，暂时这样写
    constexpr uint32_t color_shader_location=1;

    vi->Add(position_shader_location,   vertex_buffer);
    vi->Add(color_shader_location,      color_buffer);

    return vi;
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

int main(int,char **)
{
    #ifdef _DEBUG
    if(!vulkan::CheckStrideBytesByFormat())
        return 0xff;
    #endif//

    Window *win=CreateRenderWindow(OS_TEXT("VulkanTest"));

    win->Create(1280,720);

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

    //vulkan::Buffer *ubo=device->CreateUBO(1024);

    //uint8_t *p=ubo->Map();

    //if(p)
    //{
    //    memset(p,0,1024);
    //    ubo->Unmap();
    //}

    vulkan::Shader *shader=LoadShader(device->GetDevice());

    if(!shader)
        return -3;

    vulkan::Semaphore *sem=device->CreateSem();


    vulkan::VertexInput *vi=CreateVertexBuffer(device);
    const int image_count=device->GetSwapChainImageCount();
    vulkan::Framebuffer **fb=new vulkan::Framebuffer *[image_count];

    vulkan::ImageView *color_iv=device->GetColorImageView(0);
    vulkan::ImageView *depth_iv=device->GetDepthImageView();

    vulkan::PipelineCreater pc(device);

    vulkan::DescriptorSetLayoutCreater dslc(device);
    vulkan::DescriptorSetLayout *dsl=dslc.Create();
    vulkan::PipelineLayout *pl=CreatePipelineLayout(device->GetDevice(),dsl);

    pc.Set(shader);
    pc.Set(vi);
    pc.Set(PRIM_TRIANGLES);
    pc.Set(*pl);

    vulkan::Pipeline *pipeline=pc.Create();

    if(!pipeline)
        return(-4);

    device->AcquireNextImage();

    vulkan::CommandBuffer *cmd_buf=device->CreateCommandBuffer();

    cmd_buf->Begin(device->GetRenderPass(),device->GetFramebuffer(0));
    cmd_buf->Bind(pipeline);
    cmd_buf->Bind(pl);
    cmd_buf->Bind(vi);
    cmd_buf->Draw(3);
    cmd_buf->End();

    device->QueueSubmit(cmd_buf);
    device->Wait();
    device->QueuePresent();

    wait_seconds(3);

    delete vertex_buffer;
    delete color_buffer;

    delete pipeline;

    delete pl;
    delete dsl;

    delete sem;

    delete vi;
//    delete ubo;

    delete shader;

    delete cmd_buf;
    delete device;
    delete inst;
    delete win;

    return 0;
}
