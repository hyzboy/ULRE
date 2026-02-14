#pragma once

#include<hgl/type/object/TickObject.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/render/RenderFramework.h>
#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/color/Color4f.h>
#include<hgl/time/Time.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/vk/VKRenderTarget.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/mesh/Primitive.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <memory>

namespace hgl
{
    namespace graph
    {
        class RenderFramework;
        class CameraControl;

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

        graph::RenderFramework *render_framework=nullptr; // legacy entry (optional)
        graph::IGraphicsContext *graphics_context=nullptr;

        bool destroy_flag=false;
        bool render_dirty=true;
        Color4f clear_color{0,0,0,1};

    protected:

        // 以下数据在 ECS 模式下来自 ECSContext/GraphicsContext

    public:

        graph::RenderFramework *    GetRenderFramework  (){return render_framework;}
        ecs::ECSContext *           GetECSContext       (){return world.get();}
        graph::IGraphicsContext *   GetGraphicsContext  (){return graphics_context;}

        graph::VulkanDevice *       GetDevice           (){return graphics_context?graphics_context->GetDevice():(world ? world->GetGPUDevice() : (render_framework ? render_framework->GetDevice() : nullptr));}
        graph::VulkanDevAttr *      GetDevAttr          (){return graphics_context?graphics_context->GetDevAttr():(render_framework ? render_framework->GetDevAttr() : nullptr);}
        graph::TextureManager *     GetTextureManager   (){return graphics_context?graphics_context->GetTextureManager():(render_framework ? render_framework->GetTextureManager() : nullptr);}
        graph::BufferManager *      GetBufferManager    (){return graphics_context?graphics_context->GetBufferManager():(render_framework ? render_framework->GetBufferManager() : nullptr);}

        const VkExtent2D *          GetExtent           ();
        const graph::ViewportInfo * GetViewportInfo     ()const;
        graph::Camera *             GetCamera           ();
        const graph::CameraInfo *   GetCameraInfo       ()const;

        const math::Vector2i *      GetMouseCoord       ()const {return render_framework ? &render_framework->GetMouseCoord() : nullptr;}

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
        explicit WorkObject(graph::RenderFramework *);
        virtual ~WorkObject();

        virtual bool Init()=0;

        virtual void OnRenderFrameworkChange(graph::RenderFramework *rf);

        virtual void OnResize(const VkExtent2D &){}

        virtual void Tick(double);

        virtual void Render(double delta_time);

    #define FUNC_FROM_RENDER_FRAMEWORK(return_type,func_name) template<typename ...ARGS>    \
        return_type func_name(ARGS...args) \
        {   \
            return render_framework?render_framework->func_name(args...):nullptr;   \
        }

    public: // Material 相关

        FUNC_FROM_RENDER_FRAMEWORK(graph::Material *,CreateMaterial)
        FUNC_FROM_RENDER_FRAMEWORK(graph::Material *,LoadMaterial)
        FUNC_FROM_RENDER_FRAMEWORK(graph::MaterialInstance *,CreateMaterialInstance)

    public:

        FUNC_FROM_RENDER_FRAMEWORK(graph::VertexDataManager *,CreateVDM)

    public: // Buffer 相关

        FUNC_FROM_RENDER_FRAMEWORK(graph::VAB *,CreateVAB)
        FUNC_FROM_RENDER_FRAMEWORK(graph::DeviceBuffer *,CreateUBO)
        FUNC_FROM_RENDER_FRAMEWORK(graph::DeviceBuffer *,CreateSSBO)
        FUNC_FROM_RENDER_FRAMEWORK(graph::DeviceBuffer *,CreateINBO)

        FUNC_FROM_RENDER_FRAMEWORK(graph::IndexBuffer *,CreateIBO)
        FUNC_FROM_RENDER_FRAMEWORK(graph::IndexBuffer *,CreateIBO8)
        FUNC_FROM_RENDER_FRAMEWORK(graph::IndexBuffer *,CreateIBO16)
        FUNC_FROM_RENDER_FRAMEWORK(graph::IndexBuffer *,CreateIBO32)

    public: // Geometry, Primitive, Sampler 相关

        void Add(graph::Geometry *geometry)
        {
            if(!geometry)return;

            if(graphics_context)
            {
                graphics_context->Add(geometry);
                return;
            }

            if(!render_framework)return;

            render_framework->GetGeometryManager()->Add(geometry);
        }

        graph::Primitive *CreatePrimitive(graph::Geometry *geometry,graph::MaterialInstance *mi,graph::Pipeline *pipeline)
        {
            if(!geometry||!pipeline)
                return nullptr;

            if(graphics_context)
                return graphics_context->CreatePrimitive(geometry,mi,pipeline);

            if(!render_framework)
                return nullptr;

            graph::PrimitiveManager *mm = render_framework->GetPrimitiveManager();

            if(!mm)
                return nullptr;

            return mm->CreatePrimitive(geometry,mi,pipeline);
        }

        graph::Primitive *CreatePrimitive(graph::GeometryCreater *pc,graph::MaterialInstance *mi,graph::Pipeline *pipeline)
        {
            if(!pc||!pipeline)
                return nullptr;

            if(graphics_context)
                return graphics_context->CreatePrimitive(pc,mi,pipeline);

            if(!render_framework)
                return nullptr;

            graph::PrimitiveManager *mm = render_framework->GetPrimitiveManager();

            if(!mm)
                return nullptr;

            return mm->CreatePrimitive(pc,mi,pipeline);
        }

        graph::Sampler *CreateSampler(VkSamplerCreateInfo *sci=nullptr)
        {
            if(graphics_context)
                return graphics_context->CreateSampler(sci);

            return render_framework?render_framework->GetSamplerManager()->CreateSampler(sci):nullptr;
        }

        graph::Sampler *CreateSampler(graph::Texture *tex)
        {
            if(graphics_context)
                return graphics_context->CreateSampler(tex);

            return render_framework?render_framework->GetSamplerManager()->CreateSampler(tex):nullptr;
        }

        graph::Pipeline *CreatePipeline(graph::Material *mat,const graph::InlinePipeline &ip)
        {
            if(graphics_context)
                return graphics_context->CreatePipeline(mat,ip);

            return render_framework?render_framework->CreatePipeline(mat,ip):nullptr;
        }

        graph::Pipeline *CreatePipeline(graph::MaterialInstance *mi,const graph::InlinePipeline &ip)
        {
            if(graphics_context)
                return graphics_context->CreatePipeline(mi,ip);

            return render_framework?render_framework->CreatePipeline(mi,ip):nullptr;
        }

        SharedPtr<graph::GeometryCreater> GetGeometryCreater(graph::Material *mtl)
        {
            if(graphics_context)
                return graphics_context->GetGeometryCreater(mtl);

            return render_framework?render_framework->GetGeometryCreater(mtl):nullptr;
        }

        SharedPtr<graph::GeometryCreater> GetGeometryCreater(graph::MaterialInstance *mi)
        {
            if(graphics_context)
                return graphics_context->GetGeometryCreater(mi);

            return render_framework?render_framework->GetGeometryCreater(mi):nullptr;
        }

        graph::Geometry *CreateGeometry(const AnsiString &name,
                                            const uint32_t vertices_count,
                                            const graph::VIL *vil,
                                            const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
        {
            if(graphics_context)
                return graphics_context->CreateGeometry(name,vertices_count,vil,vad_list);

            return render_framework?render_framework->CreateGeometry(name,vertices_count,vil,vad_list):nullptr;
        }

        graph::Primitive *CreatePrimitive(const AnsiString &name,
                                const uint32_t vertices_count,
                                graph::MaterialInstance *mi,
                                graph::Pipeline *pipeline,
                                const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
        {
            if(graphics_context)
                return graphics_context->CreatePrimitive(name,vertices_count,mi,pipeline,vad_list);

            return render_framework?render_framework->CreatePrimitive(name,vertices_count,mi,pipeline,vad_list):nullptr;
        }

        graph::TextRender *CreateTextRender(graph::FontSource *fs,const int limit=1024)
        {
            if(graphics_context)
                return graphics_context->CreateTextRender(fs,limit);

            return render_framework?render_framework->CreateTextRender(fs,limit):nullptr;
        }

    public: // Texture 相关

        graph::Texture2D *      LoadTexture2D       (const OSString &file_name,bool auto_mipmap=true);
        graph::TextureCube *    LoadTextureCube     (const OSString &,bool auto_mipmaps=false);
        graph::Texture2DArray * CreateTexture2DArray(const AnsiString &name,const uint32_t width,const uint32_t height,const uint32_t layer,const VkFormat &fmt,bool auto_mipmaps=false);
        bool                    LoadTexture2DArray  (graph::Texture2DArray *,const uint32_t layer,const OSString &);

    #undef FUNC_FROM_RENDER_FRAMEWORK
    };//class WorkObject
}//namespcae hgl
