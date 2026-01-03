#pragma once

#include<hgl/type/object/TickObject.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/graph/SceneRenderer.h>
#include<hgl/graph/World.h>
#include<hgl/time/Time.h>
//#include<iostream>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/mesh/Primitive.h>
#include <hgl/graph/GeometryCreater.h>

namespace hgl
{
    namespace graph
    {
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

        graph::RenderFramework *render_framework=nullptr;

        bool destroy_flag=false;
        bool render_dirty=true;

    protected:

        //以下数据均取自RenderFramework

        graph::World *          world           =nullptr;           //场景
        graph::SceneRenderer *  scene_renderer  =nullptr;           //渲染器

    public:

        graph::RenderFramework *    GetRenderFramework  (){return render_framework;}
        graph::VulkanDevice *       GetDevice           (){return render_framework ? render_framework->GetDevice() : nullptr;}
        graph::VulkanDevAttr *      GetDevAttr          (){return render_framework ? render_framework->GetDevAttr() : nullptr;}
        graph::TextureManager *     GetTextureManager   (){return render_framework ? render_framework->GetTextureManager() : nullptr;}
        graph::BufferManager *      GetBufferManager    (){return render_framework ? render_framework->GetBufferManager() : nullptr;}

        const VkExtent2D *          GetExtent           (){return scene_renderer ? &scene_renderer->GetExtent() : nullptr;}

        graph::World *              GetWorld            (){return world;}
        graph::SceneNode *          GetWorldRootNode    (){return world ? world->GetRootNode() : nullptr;}
        graph::SceneRenderer *      GetSceneRenderer    (){return scene_renderer;}
        ecs::ECSContext *          GetECSContext       (){return render_framework ? render_framework->GetECSContext() : nullptr;}

        const graph::ViewportInfo * GetViewportInfo     ()const {return scene_renderer ? scene_renderer->GetViewportInfo() : nullptr;}
        graph::Camera *             GetCamera           ()      {return scene_renderer ? scene_renderer->GetCamera() : nullptr;}
        const graph::CameraInfo *   GetCameraInfo       ()const {return scene_renderer ? scene_renderer->GetCameraInfo() : nullptr;}
        graph::CameraControl *      GetCameraControl    ()      {return scene_renderer ? scene_renderer->GetCameraControl() : nullptr;}

        const math::Vector2i *      GetMouseCoord       ()const {return render_framework ? &render_framework->GetMouseCoord() : nullptr;}

    public:

        const   bool IsDestroy  ()const{return destroy_flag;}
                void MarkDestory(){destroy_flag=true;}

        const   bool IsRenderDirty  ()const{return render_dirty;}
                void MarkRenderDirty(){render_dirty=true;}

    public:

        WorkObject(graph::RenderFramework *,graph::SceneRenderer *r=nullptr);
        virtual ~WorkObject();

        virtual bool Init()=0;

        virtual void OnSceneRendererChange(graph::RenderFramework *rf,graph::SceneRenderer *r);
        
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

            if(!render_framework)return;

            render_framework->GetGeometryManager()->Add(geometry);
        }

        graph::Primitive *CreatePrimitive(graph::Geometry *geometry,graph::MaterialInstance *mi,graph::Pipeline *pipeline)
        {
            if(!geometry||!pipeline)
                return nullptr;

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

            if(!render_framework)
                return nullptr;

            graph::PrimitiveManager *mm = render_framework->GetPrimitiveManager();

            if(!mm)
                return nullptr;

            return mm->CreatePrimitive(pc,mi,pipeline);
        }

        graph::Sampler *CreateSampler(VkSamplerCreateInfo *sci=nullptr)
        {
            return render_framework?render_framework->GetSamplerManager()->CreateSampler(sci):nullptr;
        }

        graph::Sampler *CreateSampler(graph::Texture *tex)
        {
            return render_framework?render_framework->GetSamplerManager()->CreateSampler(tex):nullptr;
        }

        FUNC_FROM_RENDER_FRAMEWORK(graph::Pipeline *,CreatePipeline)
        FUNC_FROM_RENDER_FRAMEWORK(SharedPtr<graph::GeometryCreater>,GetGeometryCreater)

        graph::Geometry *CreateGeometry(const AnsiString &name,
                                            const uint32_t vertices_count,
                                            const graph::VIL *vil,
                                            const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
        {
            return render_framework?render_framework->CreateGeometry(name,vertices_count,vil,vad_list):nullptr;
        }

        graph::Primitive *CreatePrimitive(const AnsiString &name,
                                const uint32_t vertices_count,
                                graph::MaterialInstance *mi,
                                graph::Pipeline *pipeline,
                                const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
        {
            return render_framework?render_framework->CreatePrimitive(name,vertices_count,mi,pipeline,vad_list):nullptr;
        }

        graph::TextRender *CreateTextRender(graph::FontSource *fs,const int limit=1024)
        {
            return render_framework?render_framework->CreateTextRender(fs,limit):nullptr;
        }

    public: // Texture 相关

        graph::Texture2D *      LoadTexture2D       (const OSString &file_name,bool auto_mipmap=true);
        graph::TextureCube *    LoadTextureCube     (const OSString &,bool auto_mipmaps=false);
        graph::Texture2DArray * CreateTexture2DArray(const AnsiString &name,const uint32_t width,const uint32_t height,const uint32_t layer,const VkFormat &fmt,bool auto_mipmaps=false);
        bool                    LoadTexture2DArray  (graph::Texture2DArray *,const uint32_t layer,const OSString &);

    public: //Component 相关

        template<typename C,typename ...ARGS>
        inline C *CreateComponent(const graph::CreateComponentInfo *cci,ARGS...args)
        {
            return render_framework?render_framework->CreateComponent<C>(cci,args...):nullptr; //创建组件
        }

    #undef FUNC_FROM_RENDER_FRAMEWORK
    };//class WorkObject
}//namespcae hgl
