#include<hgl/WorkObject.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/GeometryCreater.h>
#include<hgl/graph/VKRenderTargetSwapchain.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/font/TextRender.h>
#include<hgl/Time.h>
//#include<iostream>

namespace hgl
{
    WorkObject::WorkObject(graph::RenderFramework *rf,graph::SceneRenderer *r)
    {
        if(!r)
            scene_renderer=rf->GetDefaultSceneRenderer();

        OnSceneRendererChange(rf,scene_renderer);
    }

    WorkObject::~WorkObject()
    {
    }

    void WorkObject::OnSceneRendererChange(graph::RenderFramework *rf,graph::SceneRenderer *r)
    {
        if(!rf)
        {
            render_framework=nullptr;
        }

        if(!rf||!r)
        {
            return;
        }

        render_framework=rf;

        scene=rf->GetDefaultWorld();
        scene_renderer=rf->GetDefaultSceneRenderer();
    }

    void WorkObject::Tick(double delta)
    {
        if(scene_renderer)
            scene_renderer->Tick(delta);
    }

    void WorkObject::Render(double delta_time)
    {
        if(!scene_renderer)
        {
            //std::cerr<<"WorkObject::Render,scene_renderer=nullptr"<<std::endl;
            return;
        }

        //std::cout<<"WorkObject::Render begin, render_dirty="<<(render_dirty?"true":"false")<<std::endl;

        if(render_dirty)
        {
            scene_renderer->RenderFrame();

            render_dirty=false;
        }

        scene_renderer->Submit();

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

    graph::TextureCube *WorkObject::LoadTextureCube(const OSString &filename,bool auto_mipmaps)
    {
        if(filename.IsEmpty())
            return(nullptr);

        auto tm=GetTextureManager();

        if(!tm)return(nullptr);

        return tm->LoadTextureCube(filename,auto_mipmaps);
    }

    graph::Texture2DArray * WorkObject::CreateTexture2DArray(const AnsiString &name,const uint32_t width,const uint32_t height,const uint32_t layer,const VkFormat &fmt,bool auto_mipmaps)
    {
        if(name.IsEmpty())
            return(nullptr);

        auto tm=GetTextureManager();

        if(!tm)return(nullptr);

        return tm->CreateTexture2DArray(name,width,height,layer,fmt,auto_mipmaps);
    }

    bool WorkObject::LoadTexture2DArray(graph::Texture2DArray *tex_array,const uint32_t layer,const OSString &filename)
    {
        if(!tex_array||filename.IsEmpty())
            return(false);

        auto tm=GetTextureManager();

        if(!tm)return(false);

        return tm->LoadTexture2DArray(tex_array,layer,filename);
    }
}//namespcae hgl
