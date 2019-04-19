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

#include<io.h>
#include<fcntl.h>
#include<stdlib.h>

using namespace hgl;
using namespace hgl::graph;

VkShaderModule vs=nullptr;
VkShaderModule fs=nullptr;

char *LoadFile(const char *filename,uint32_t &file_length)
{
    int fp=_open(filename,O_RDONLY|O_BINARY);

    if(fp==-1)return(nullptr);

    file_length=_filelength(fp);
    char *data=new char[file_length];

    const int result=_read(fp,data,file_length);
    
    if(result!=file_length)
    {
        delete[] data;
        return(nullptr);
    }

    _close(fp);
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

    vulkan::Buffer *ubo=device->CreateUBO(1024);

    uint8_t *p=ubo->Map();

    if(p)
    {
        memset(p,0,1024);
        ubo->Unmap();
    }

    vulkan::Shader *shader=LoadShader(device->GetDevice());

    if(!shader)
        return -3;

    vulkan::Fence *fence=device->CreateFence();
    vulkan::Semaphore *sem=device->CreateSem();

    vulkan::VertexInput vi;
    vulkan::PipelineCreater pc(device);
    vulkan::RenderPass *rp=device->CreateRenderPass();

    vulkan::DescriptorSetLayoutCreater dslc(device);
    vulkan::DescriptorSetLayout *dsl=dslc.Create();
    vulkan::PipelineLayout *pl=CreatePipelineLayout(device->GetDevice(),dsl);

    pc.Set(shader);
    pc.Set(&vi);
    pc.Set(PRIM_TRIANGLES);
    pc.Set(*pl);
    pc.Set(*rp);

    vulkan::Pipeline *pipeline=pc.Create();

    if(pipeline)
    {
        delete pipeline;
    }

    delete sem;
    delete fence;
    delete rp;

    delete ubo;

    delete cmd_buf;
    delete device;
    delete inst;
    delete win;

    return 0;
}
