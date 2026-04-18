#pragma once

#include<hgl/object/TickObject.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/color/Color4f.h>
#include<hgl/vk/VKRenderTarget.h>
#include <memory>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
        class VILConfig;
        class Camera;
        struct ViewportInfo;
        struct CameraInfo;

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

        graph::RenderContext *render_context=nullptr;

        bool destroy_flag=false;
        bool render_dirty=true;
        Color4f clear_color{0,0,0,1};

    protected:

        // 以下数据在 ECS 模式下来自 ECSContext/GraphicsContext

    public:

        ecs::ECSContext *           GetECSContext       (){return world.get();}
        graph::RenderContext *      GetRenderContext    (){return render_context;}
        graph::GraphicsContext *    GetGraphicsContext  ()
        {
            if (render_context)
                return render_context->GetGraphicsContext();

            if (world)
                return world->GetGraphicsContext();

            return nullptr;
        }

        graph::VulkanDevice *       GetDevice           ()
        {
            if (auto *gc = GetGraphicsContext())
                return gc->GetDevice();
            if (world && world->GetGPUDevice())
                return world->GetGPUDevice();
            return nullptr;
        }
        graph::VulkanDevAttr *      GetDevAttr          ()
        {
            auto *device = GetDevice();
            return device ? device->GetDevAttr() : nullptr;
        }
        graph::TextureManager *     GetTextureManager   ()
        {
            if (auto *gc = GetGraphicsContext())
                return gc->GetTextureManager();
            return nullptr;
        }
        graph::BufferManager *      GetBufferManager    ()
        {
            if (auto *gc = GetGraphicsContext())
                return gc->GetBufferManager();
            return nullptr;
        }
        graph::ShaderMaterialProgramManager *    GetMaterialManager  ()
        {
            if (auto *gc = GetGraphicsContext())
                return gc->GetMaterialManager();
            return nullptr;
        }
        graph::SamplerManager *     GetSamplerManager   ()
        {
            if (auto *gc = GetGraphicsContext())
                return gc->GetSamplerManager();
            return nullptr;
        }
        graph::GeometryManager *    GetGeometryManager  ()
        {
            if (auto *gc = GetGraphicsContext())
                return gc->GetGeometryManager();
            return nullptr;
        }
        graph::PrimitiveManager *   GetPrimitiveManager ()
        {
            if (auto *gc = GetGraphicsContext())
                return gc->GetPrimitiveManager();
            return nullptr;
        }
        graph::MaterialRecipeRegistry *GetMaterialAssetRegistry()
        {
            if (auto *gc = GetGraphicsContext())
                return gc->GetMaterialAssetRegistry();

            return nullptr;
        }
        graph::MaterialBindingInstance *ResolveOrCreateBindingInstance(const graph::mtl::MaterialRecipe &rec,
                                           const void *instance_data = nullptr,
                                           uint32 instance_data_size = 0,
                                           graph::MaterialDomainHandle *out_handle = nullptr)
        {
            auto *registry = GetMaterialAssetRegistry();
            if (!registry)
                return nullptr;

            return registry->ResolveOrCreateBindingInstance(rec, instance_data, instance_data_size, out_handle);
        }

        graph::MaterialBindingInstance *ResolveOrCreateBindingInstance(const graph::mtl::MaterialRecipe &rec,
                                           const graph::GeometryVertexFormat &gvf,
                                           const void *instance_data = nullptr,
                                           uint32 instance_data_size = 0,
                                           graph::MaterialDomainHandle *out_handle = nullptr)
        {
            auto *registry = GetMaterialAssetRegistry();
            if (!registry)
                return nullptr;

            return registry->ResolveOrCreateBindingInstance(rec, gvf, instance_data, instance_data_size, out_handle);
        }

        const VkExtent2D *          GetExtent           ();
        const graph::ViewportInfo * GetViewportInfo     ()const;
        graph::Camera *             GetCamera           ();
        const graph::CameraInfo *   GetCameraInfo       ()const;

        const math::Vector2i *      GetMouseCoord       ()const;

        void SetClearColor(const Color4f &color) { clear_color = color; }
        const Color4f &GetClearColor() const { return clear_color; }

    public:

        const   bool IsDestroy  ()const{return destroy_flag;}
                void MarkDestory(){destroy_flag=true;}

        const   bool IsRenderDirty  ()const{return render_dirty;}
                void MarkRenderDirty(){render_dirty=true;}
            void ClearRenderDirty(){render_dirty=false;}

    protected:

        // 保护的默认构造函数，用于子类或框架初始化
        WorkObject() : world(nullptr), render_context(nullptr) {}

    public:

        explicit WorkObject(std::shared_ptr<ecs::ECSContext> ctx);

        virtual ~WorkObject()=default;

        // 内部初始化函数，仅由AppFramework/WorkManager调用
        // DO NOT USE DIRECTLY IN APPLICATION CODE
        void _InitializeWithECSContext_INTERNAL_DO_NOT_CALL(std::shared_ptr<ecs::ECSContext> ctx);

        virtual bool Init()=0;

        virtual void OnResize(const VkExtent2D &){}

        virtual void Tick(double);

        virtual void Render(double delta_time);

    public:

        // Use RenderContext/GraphicsContext directly for resource creation.
    };//class WorkObject
}//namespcae hgl
