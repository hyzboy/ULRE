#pragma once

#include<hgl/object/TickObject.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/color/Color4f.h>
#include<hgl/vk/VKRenderTarget.h>
#include <memory>

namespace hgl
{
    using PrimitiveVertexWrite = graph::GraphicsGeometryFactory::VertexAttribWrite;

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
        graph::MaterialManager *    GetMaterialManager  ()
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
        graph::MaterialAssetRegistry *GetMaterialAssetRegistry()
        {
            if (auto *gc = GetGraphicsContext())
                return gc->GetMaterialAssetRegistry();

            return nullptr;
        }
        // Handle-first helper: allocate stable material handle directly from MaterialAssetRecord.
        // This is the preferred replacement for legacy AcquireMI/CreateMI callsites.
        graph::MaterialInstanceHandle AllocateMaterialHandle(const graph::mtl::MaterialAssetRecord &rec,
                                                             const void *instance_data = nullptr,
                                                             uint32 instance_data_size = 0,
                                                             graph::MaterialDomainHandle *out_handle = nullptr,
                                                             const graph::VIL *override_vil = nullptr)
        {
            auto *registry = GetMaterialAssetRegistry();
            if (!registry)
                return graph::InvalidMaterialInstanceHandle;

            graph::MaterialDomainHandle handle = registry->Acquire(rec);
            if (!handle.IsValid())
                return graph::InvalidMaterialInstanceHandle;

            if (out_handle)
                *out_handle = handle;

            const graph::VIL *resolved_vil = override_vil
                                           ? override_vil
                                           : registry->ResolveVIL(handle.material, rec, nullptr);

            if (!resolved_vil && handle.material)
                resolved_vil = handle.material->GetDefaultVIL();

            if (!handle.material || !resolved_vil)
                return graph::InvalidMaterialInstanceHandle;

            graph::MaterialBindingInit init;
            init.material = handle.material;
            init.domain = handle.domain;
            init.vil = resolved_vil;
            init.preset = rec.pipeline;
            init.material_preset = rec.preset;
            init.instance_data = instance_data;
            init.instance_data_size = instance_data_size;

            return registry->AllocateHandle(init);
        }

        bool BuildMaterialSlot(const graph::MaterialInstanceHandle handle,
                               graph::PrimitiveMaterialSlot &out_slot) const
        {
            auto *registry = const_cast<WorkObject *>(this)->GetMaterialAssetRegistry();
            if (!registry)
                return false;

            return registry->BuildSlot(handle, out_slot);
        }

        bool WriteMaterialData(const graph::MaterialInstanceHandle handle,
                               const void *instance_data,
                               const uint32 instance_data_size)
        {
            auto *registry = GetMaterialAssetRegistry();
            if (!registry)
                return false;

            return registry->WriteMIData(handle, instance_data, instance_data_size);
        }

        bool SetMaterialTextureArrayLayer(const graph::MaterialInstanceHandle handle,
                                          const graph::mtl::SamplerSlot slot,
                                          const uint32 layer)
        {
            auto *registry = GetMaterialAssetRegistry();
            if (!registry)
                return false;

            return registry->SetTextureArrayLayer(handle, slot, layer);
        }

        bool ReleaseMaterialHandle(const graph::MaterialInstanceHandle handle)
        {
            auto *registry = GetMaterialAssetRegistry();
            if (!registry)
                return false;

            return registry->ReleaseHandle(handle);
        }

        // Semantic-path helper: register semantic material directly from MaterialAssetRecord.
        graph::SemanticMaterialId RegisterSemanticMaterial(const graph::mtl::MaterialAssetRecord &rec)
        {
            auto *registry = GetMaterialAssetRegistry();
            if (!registry)
                return 0;

            return registry->RegisterSemanticMaterial(rec);
        }

        graph::Primitive *CreateSemanticPrimitive(graph::SemanticMaterialId semantic_id,
                                                  const AnsiString &geometry_name,
                                                  uint32 vertex_count,
                                                  std::initializer_list<PrimitiveVertexWrite> vertex_writes)
        {
            auto *graphics_context = GetGraphicsContext();
            if (!graphics_context)
                return nullptr;

            return graph::GraphicsGeometryFactory::CreatePrimitive(graphics_context,
                                                                   semantic_id,
                                                                   geometry_name,
                                                                   vertex_count,
                                                                   vertex_writes);
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
