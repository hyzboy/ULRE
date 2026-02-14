#include<hgl/WorkObject.h>
#include<hgl/graph/render/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/font/TextRender.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/time/Time.h>
//#include<iostream>

namespace hgl
{
    WorkObject::WorkObject(graph::RenderFramework *rf)
    {
        OnSceneRendererChange(rf);
    }

    WorkObject::WorkObject(std::shared_ptr<ecs::ECSContext> ctx)
        : world(std::move(ctx))
    {
        if (world)
        {
            graphics_context = world->GetGraphicsContext();
        }
    }

    WorkObject::~WorkObject()
    {
    }

    graph::Camera *WorkObject::GetCamera()
    {
        if (world)
        {
            auto camera_system = world->GetSystem<ecs::CameraSystem>();
            return camera_system ? camera_system->GetCamera() : nullptr;
        }

        return nullptr;
    }

    const graph::CameraInfo *WorkObject::GetCameraInfo() const
    {
        if (world)
        {
            auto camera_system = world->GetSystem<ecs::CameraSystem>();
            return camera_system ? camera_system->GetCameraInfo() : nullptr;
        }

        return nullptr;
    }

    const VkExtent2D *WorkObject::GetExtent()
    {
        if (world)
        {
            auto target = world->GetRenderTarget();
            return target ? &target->GetExtent() : nullptr;
        }

        return nullptr;
    }

    const graph::ViewportInfo *WorkObject::GetViewportInfo() const
    {
        if (world)
        {
            auto camera_system = world->GetSystem<ecs::CameraSystem>();
            if (camera_system)
                return camera_system->GetViewportInfo();

            auto target = world->GetRenderTarget();
            return target ? target->GetViewportInfo() : nullptr;
        }

        return nullptr;
    }

    void WorkObject::OnSceneRendererChange(graph::RenderFramework *rf)
    {
        if(!rf)
        {
            render_framework=nullptr;
            graphics_context=nullptr;
            world.reset();
            return;
        }

        render_framework=rf;
        world.reset();
        if (rf->GetECSContext())
            world = std::shared_ptr<ecs::ECSContext>(rf->GetECSContext(), [](ecs::ECSContext*){});
        graphics_context=world?world->GetGraphicsContext():nullptr;
    }

    void WorkObject::Tick(double delta)
    {
        if (world)
        {
            world->Tick(static_cast<float>(delta));
            return;
        }

    }

    void WorkObject::Render(double delta_time)
    {
        if (world)
            return;
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
