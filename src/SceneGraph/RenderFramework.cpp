#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/RenderModule.h>

VK_NAMESPACE_BEGIN

void RenderFramework::MainLoop()
{
    for(auto rm:module_list)
    {
        if(rm->IsEnable())
            rm->OnPreFrame();
    }

    BeginFrame();

    for(auto rm:module_list)
    {
        if(rm->IsEnable())
            rm->Execute(0,nullptr);
    }

    for(auto rm:module_list)
    {
        if(rm->IsEnable())
            rm->OnPostFrame();
    }

    EndFrame();
}

VK_NAMESPACE_END
