#pragma once

#include<hgl/type/object/TickObject.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/graph/Renderer.h>
#include<hgl/graph/Scene.h>
#include<hgl/Time.h>
//#include<iostream>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/Mesh.h>
#include <hgl/graph/PrimitiveCreater.h>

namespace hgl
{
    namespace graph
    {
        class Texture2D;
        class Texture2DArray;
        class TextureCube;
        class Primitive;
        class PrimitiveCreater;
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

        graph::Scene *          scene   =nullptr;           //场景
        graph::Renderer *       renderer=nullptr;           //渲染器

    public:

        graph::RenderFramework *    GetRenderFramework  (){return render_framework;}
        graph::VulkanDevice *       GetDevice           (){return render_framework->GetDevice();}
        graph::VulkanDevAttr *      GetDevAttr          (){return render_framework->GetDevAttr();}
        graph::TextureManager *     GetTextureManager   (){return render_framework->GetTextureManager();}
        graph::BufferManager *      GetBufferManager    (){return render_framework->GetBufferManager();}

        const VkExtent2D &          GetExtent           (){return renderer->GetExtent();}

        graph::Scene *              GetScene            (){return scene;}
        graph::SceneNode *          GetSceneRoot        (){return scene->GetRootNode();}
        graph::Renderer *           GetRenderer         (){return renderer;}
        graph::Camera *             GetCamera           (){return renderer->GetCamera();}
        graph::CameraControl *      GetCameraControl    (){return render_framework->GetDefaultCameraControl();}

        bool                        GetMouseCoord       (Vector2i *mc)const{return render_framework->GetMouseCoord(mc);}

    public:

        const   bool IsDestroy  ()const{return destroy_flag;}
                void MarkDestory(){destroy_flag=true;}

        const   bool IsRenderDirty  ()const{return render_dirty;}
                void MarkRenderDirty(){render_dirty=true;}

    public:

        WorkObject(graph::RenderFramework *,graph::Renderer *r=nullptr);
        virtual ~WorkObject()=default;

        virtual bool Init()=0;

        virtual void OnRendererChange(graph::RenderFramework *rf,graph::Renderer *r);
        
        virtual void OnResize(const VkExtent2D &){}

        virtual void Tick(double){}

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

    public: // Primitive, Mesh, Sampler 相关

        void Add(graph::Primitive *prim)
        {
            if(!prim)return;

            if(!render_framework)return;

            render_framework->GetPrimitiveManager()->Add(prim);
        }

        graph::Mesh *CreateMesh(graph::Primitive *prim,graph::MaterialInstance *mi,graph::Pipeline *pipeline)
        {
            if(!prim||!pipeline)
                return nullptr;

            if(!render_framework)
                return nullptr;
            
            graph::MeshManager *mm = render_framework->GetMeshManager();

            if(!mm)
                return nullptr;
                
            return mm->CreateMesh(prim,mi,pipeline);
        }

        graph::Mesh *CreateMesh(graph::PrimitiveCreater *pc,graph::MaterialInstance *mi,graph::Pipeline *pipeline)
        {
            if(!pc||!pipeline)
                return nullptr;

            if(!render_framework)
                return nullptr;

            graph::MeshManager *mm = render_framework->GetMeshManager();

            if(!mm)
                return nullptr;

            return mm->CreateMesh(pc,mi,pipeline);
        }

        graph::Sampler *CreateSampler(VkSamplerCreateInfo *sci=nullptr)
        {
            return render_framework?render_framework->GetSamplerManager()->CreateSampler(sci):nullptr;
        }

        graph::Sampler *CreateSampler(graph::Texture *tex)
        {
            return render_framework?render_framework->GetSamplerManager()->CreateSampler(tex):nullptr;
        }

    #define WO_FUNC_FROM_RENDER_FRAMEWORK(name,return_type) template<typename ...ARGS> return_type name(ARGS...args){return render_framework?render_framework->name(args...):nullptr;}

        WO_FUNC_FROM_RENDER_FRAMEWORK(CreatePipeline,graph::Pipeline *)
        WO_FUNC_FROM_RENDER_FRAMEWORK(GetPrimitiveCreater,SharedPtr<graph::PrimitiveCreater>)

    #undef WO_FUNC_FROM_RENDER_FRAMEWORK

        graph::Primitive *CreatePrimitive(const AnsiString &name,
                                            const uint32_t vertices_count,
                                            const graph::VIL *vil,
                                            const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
        {
            return render_framework?render_framework->CreatePrimitive(name,vertices_count,vil,vad_list):nullptr;
        }

        graph::Mesh *CreateMesh(const AnsiString &name,
                                const uint32_t vertices_count,
                                graph::MaterialInstance *mi,
                                graph::Pipeline *pipeline,
                                const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
        {
            return render_framework?render_framework->CreateMesh(name,vertices_count,mi,pipeline,vad_list):nullptr;
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
    };//class WorkObject

    /**
    * 但我们认为游戏开发者不应该关注如何控制渲染，而应该关注如何处理游戏逻辑.
    * 所以我们在WorkObject的基础上再提供RenderWorkObject派生类，用于直接封装好的渲染场景树控制。
    * 
    * 开发者仅需要将要渲染的物件放置于场景树即可。

    * 但开发者也可以直接使用WorkObject，自行管理这些事。
    * */
}//namespcae hgl
