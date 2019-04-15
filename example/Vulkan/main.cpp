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
    int fp=_open(filename,O_RDONLY);

    if(fp==-1)return(nullptr);

    file_length=_filelength(fp);
    char *data=new char[file_length];

    if(_read(fp,data,file_length)!=file_length)
    {
        delete[] data;
        return(nullptr);
    }

    _close(fp);
    return data;
}

bool LoadShader(VkDevice device)
{
}

int main(int,char **)
{
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
