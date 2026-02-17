#pragma once

#include<hgl/type/object/TickObject.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/platform/AppFramework.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/color/Color4f.h>
#include<hgl/vk/VKRenderTarget.h>
#include <memory>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
        class VILConfig;

        class Texture2D;
        class Texture2DArray;
        class TextureCube;
        class Geometry;
        class GeometryCreater;
        class Sampler;
        class Texture;

        namespace mtl
        {
            class MaterialCreateInfo;
            struct Material2DCreateConfig;
            struct Material3DCreateConfig;
        }
    }

    /**
    * 工作对象</p>
    *
    * WorkObject被定义为工作对象，所有的渲染控制都需要被写在WorkObject的Render函数下。
    */
    class WorkObject:public TickObject
    {
    protected:

        OBJECT_LOGGER

    private:

        std::shared_ptr<ecs::ECSContext> world;

        AppFramework *app_framework=nullptr; // Application framework (optional)
        graph::RenderContext *render_context=nullptr;

        bool destroy_flag=false;
        bool render_dirty=true;
        Color4f clear_color{0,0,0,1};

    protected:

        // 以下数据在 ECS 模式下来自 ECSContext/GraphicsContext

    public:

        AppFramework *              GetAppFramework     (){return app_framework;}
        ecs::ECSContext *           GetECSContext       (){return world.get();}
        graph::RenderContext *      GetRenderContext    (){return render_context;}

        graph::VulkanDevice *       GetDevice           ()
        {
            if (render_context)
            {
                if (auto *gc = render_context->GetGraphicsContext())
                    return gc->GetDevice();
            }
            if (world && world->GetGPUDevice())
                return world->GetGPUDevice();
            return app_framework ? app_framework->GetDevice() : nullptr;
        }
        graph::VulkanDevAttr *      GetDevAttr          ()
        {
            auto *device = GetDevice();
            return device ? device->GetDevAttr() : nullptr;
        }
        graph::TextureManager *     GetTextureManager   ()
        {
            if (render_context)
            {
                if (auto *gc = render_context->GetGraphicsContext())
                    return gc->GetTextureManager();
            }
            if (app_framework)
            {
                if (auto *gc = app_framework->GetGraphicsContext())
                    return gc->GetTextureManager();
            }
            return nullptr;
        }
        graph::BufferManager *      GetBufferManager    ()
        {
            if (render_context)
            {
                if (auto *gc = render_context->GetGraphicsContext())
                    return gc->GetBufferManager();
            }
            if (app_framework)
            {
                if (auto *gc = app_framework->GetGraphicsContext())
                    return gc->GetBufferManager();
            }
            return nullptr;
        }

        const VkExtent2D *          GetExtent           ();
        const graph::ViewportInfo * GetViewportInfo     ()const;
        graph::Camera *             GetCamera           ();
        const graph::CameraInfo *   GetCameraInfo       ()const;

        const math::Vector2i *      GetMouseCoord       ()const {return app_framework ? &app_framework->GetMouseCoord() : nullptr;}

        void SetClearColor(const Color4f &color) { clear_color = color; }
        const Color4f &GetClearColor() const { return clear_color; }

    public:

        const   bool IsDestroy  ()const{return destroy_flag;}
                void MarkDestory(){destroy_flag=true;}

        const   bool IsRenderDirty  ()const{return render_dirty;}
                void MarkRenderDirty(){render_dirty=true;}
            void ClearRenderDirty(){render_dirty=false;}

    public:

        explicit WorkObject(std::shared_ptr<ecs::ECSContext> ctx);
        explicit WorkObject(AppFramework *);
        virtual ~WorkObject();

        virtual bool Init()=0;

        virtual void OnAppFrameworkChange(AppFramework *af);

        virtual void OnResize(const VkExtent2D &){}

        virtual void Tick(double);

        virtual void Render(double delta_time);

    public:

        // Use RenderContext/GraphicsContext directly for resource creation.
    };//class WorkObject
}//namespcae hgl
