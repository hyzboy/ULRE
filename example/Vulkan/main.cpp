#include"Window.h"
#include"VKInstance.h"
#include"RenderSurface.h"
#include"VKShader.h"

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

bool LoadShader(vulkan::ShaderCreater *sc,const char *filename,VkShaderStageFlagBits shader_flag)
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

bool LoadShader(VkDevice device)
{
    vulkan::ShaderCreater sc(device);

    if(!LoadShader(&sc,"FlatColor.vert.spv",VK_SHADER_STAGE_VERTEX_BIT))
        return(false);

    if(!LoadShader(&sc,"FlatColor.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT))
        return(false);

    return(true);
}

#ifdef _DEBUG
VK_NAMESPACE_BEGIN
bool CheckStrideBytesByFormat();
VK_NAMESPACE_END
#endif//

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

    vulkan::RenderSurface *render=inst->CreateSurface(win);

    if(!render)
    {
        delete inst;
        delete win;
        return(-2);
    }

    {
        const vulkan::PhysicalDevice *render_device=render->GetPhysicalDevice();

        std::cout<<"auto select physical device: "<<render_device->GetDeviceName()<<std::endl;
    }

    if(!LoadShader(render->GetDevice()))
        return(-3);

    vulkan::CommandBuffer *cmd_buf=render->CreateCommandBuffer();

    vulkan::Buffer *ubo=render->CreateUBO(1024);

    uint8_t *p=ubo->Map();

    if(p)
    {
        memset(p,0,1024);
        ubo->Unmap();
    }

    vulkan::RenderPass *rp=render->CreateRenderPass();

    delete rp;

    delete ubo;

    delete cmd_buf;
    delete render;
    delete inst;
    delete win;

    return 0;
}
