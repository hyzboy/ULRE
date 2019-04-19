#include"Window.h"
#include"VKInstance.h"
#include"VKPhysicalDevice.h"
#include"VKDevice.h"
#include"VKBuffer.h"
#include"VKShader.h"
#include"VKVertexInput.h"
#include"VKDescriptorSets.h"
#include"VKRenderPass.h"
#include"VKPipelineLayout.h"
#include"VKPipeline.h"
#include"VKCommandBuffer.h"
#include"VKFence.h"
#include"VKSemaphore.h"
#include"VKFormat.h"
#include"VKFramebuffer.h"

#include<fstream>

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

vulkan::VertexInput *CreateVertexBuffer(vulkan::Device *dev)
{
    vulkan::VertexBuffer *vb=dev->CreateVBO(FMT_RG32F,6*sizeof(float));
    vulkan::VertexBuffer *cb=dev->CreateVBO(FMT_RGB32F,9*sizeof(float));

    {
        float *p=(float *)vb->Map();

        memcpy(p,vertex_data,6*sizeof(float));

        vb->Unmap();
    }

    {
        float *p=(float *)cb->Map();

        memcpy(p,color_data,9*sizeof(float));

        cb->Unmap();
    }

    vulkan::VertexInput *vi=new vulkan::VertexInput();

    constexpr uint32_t position_shader_location=0;          //对应shader中的layout(locaiton=0，暂时这样写
    constexpr uint32_t color_shader_location=1;

    vi->Add(position_shader_location,   vb);
    vi->Add(color_shader_location,      cb);

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

    vulkan::CommandBuffer *cmd_buf=device->CreateCommandBuffer();

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

    vulkan::Fence *fence=device->CreateFence();
    vulkan::Semaphore *sem=device->CreateSem();

    vulkan::VertexInput *vi=CreateVertexBuffer(device);

    vulkan::PipelineCreater pc(device);
    vulkan::RenderPass *rp=device->CreateRenderPass();

    vulkan::DescriptorSetLayoutCreater dslc(device);
    vulkan::DescriptorSetLayout *dsl=dslc.Create();
    vulkan::PipelineLayout *pl=CreatePipelineLayout(device->GetDevice(),dsl);

    pc.Set(shader);
    pc.Set(vi);
    pc.Set(PRIM_TRIANGLES);
    pc.Set(*pl);
    pc.Set(*rp);

    vulkan::Pipeline *pipeline=pc.Create();

    if(!pipeline)
        return(-4);

    device->AcquireNextImage();     //这样才会得到current_framebuffer的值，下面的device->GetColorImageView()才会正确

    vulkan::Framebuffer *fb=vulkan::CreateFramebuffer(device,rp,device->GetColorImageView(),device->GetDepthImageView());

    cmd_buf->Begin(rp,fb);
    cmd_buf->Bind(pipeline);
    cmd_buf->Bind(pl);
    cmd_buf->Bind(vi);
    cmd_buf->Draw(3);
    cmd_buf->End();

    device->QueueSubmit(cmd_buf,fence);
    device->Wait(fence);
    device->QueuePresent();

    wait_seconds(3);

    delete pipeline;

    delete sem;
    delete fence;
    delete rp;

    delete vi;
//    delete ubo;

    delete cmd_buf;
    delete device;
    delete inst;
    delete win;

    return 0;
}
