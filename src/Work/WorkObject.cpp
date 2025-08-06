#include<hgl/WorkObject.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKRenderTargetSwapchain.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/Time.h>
//#include<iostream>

namespace hgl
{
    WorkObject::WorkObject(graph::RenderFramework *rf,graph::Renderer *r)
    {
        if(!r)
            renderer=rf->GetDefaultRenderer();

        OnRendererChange(rf,renderer);
    }

    void WorkObject::OnRendererChange(graph::RenderFramework *rf,graph::Renderer *r)
    {
        if(!rf)
        {
            render_framework=nullptr;
            db=nullptr;
        }

        if(!rf||!r)
        {
            return;
        }

        render_framework=rf;

        db=rf->GetRenderResource();
        scene=rf->GetDefaultScene();
        renderer=rf->GetDefaultRenderer();
    }

    void WorkObject::Render(double delta_time)
    {
        if(!renderer)
        {
            //std::cerr<<"WorkObject::Render,renderer=nullptr"<<std::endl;
            return;
        }

        //std::cout<<"WorkObject::Render begin, render_dirty="<<(render_dirty?"true":"false")<<std::endl;

        if(render_dirty)
        {
            renderer->RenderFrame();

            render_dirty=false;
        }

        renderer->Submit();

        //std::cout<<"WorkObject::Render End"<<std::endl;
    }

    graph::Texture2D *WorkObject::LoadTexture2D(const OSString &filename,bool auto_mipmap)
    {
        if(filename.IsEmpty())
            return(nullptr);

        auto tm=GetTextureManager();

        if(!tm)
        {
            //hgl::LogError(OS_TEXT("WorkObject::LoadTexture2D,GetTextureManager() is nullptr!"));
            return(nullptr);
        }

        return tm->LoadTexture2D(filename,auto_mipmap);
    }
}//namespcae hgl
