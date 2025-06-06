#pragma once
#include<hgl/type/object/TickObject.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/graph/Renderer.h>
#include<hgl/graph/Scene.h>
#include<hgl/Time.h>
//#include<iostream>

namespace hgl
{
    namespace graph::mtl
    {
        class MaterialCreateInfo;
    }

    /**
    * 工作对象</p>
    * 
    * WorkObject被定义为工作对象，所有的渲染控制都需要被写在WorkObject的Render函数下。
    */
    class WorkObject:public TickObject
    {
        graph::RenderFramework *render_framework=nullptr;

        bool destroy_flag=false;
        bool render_dirty=true;

    protected:

        //以下数据均取自RenderFramework

        graph::RenderResource *db=nullptr;                  //暂时的，未来会被更好的机制替代

        graph::Scene *      scene=nullptr;                  //场景
        graph::Renderer *   renderer=nullptr;               //渲染器

    public:

        graph::RenderFramework *    GetRenderFramework  (){return render_framework;}
        graph::VulkanDevice *       GetDevice           (){return render_framework->GetDevice();}
        graph::VulkanDevAttr *      GetDevAttr          (){return render_framework->GetDevAttr();}
        graph::TextureManager *     GetTextureManager   (){return render_framework->GetTextureManager();}

        const VkExtent2D &          GetExtent           (){return renderer->GetExtent();}

        graph::Scene *              GetScene            (){return scene;}
        graph::SceneNode *          GetSceneRoot        (){return scene->GetRootNode();}
        graph::Renderer *           GetRenderer         (){return renderer;}
        graph::Camera *             GetCamera           (){return renderer->GetCurCamera();}

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

    public:

        template<typename ...ARGS>
        graph::Pipeline *CreatePipeline(ARGS...args)
        {
            return renderer->GetRenderPass()->CreatePipeline(args...);
        }

        graph::MaterialInstance *CreateMaterialInstance(const AnsiString &mi_name,const graph::mtl::MaterialCreateInfo *mci,const graph::VILConfig *vil_cfg=nullptr)
        {
            return db->CreateMaterialInstance(mi_name,mci,vil_cfg);
        }

        graph::MaterialInstance *CreateMaterialInstance(const AnsiString &mtl_name,graph::mtl::MaterialCreateConfig *mtl_cfg,const graph::VILConfig *vil_cfg=nullptr)
        {            
            AutoDelete<graph::mtl::MaterialCreateInfo> mci=graph::mtl::CreateMaterialCreateInfo(GetDevAttr(),mtl_name,mtl_cfg);

            return db->CreateMaterialInstance(mtl_name,mci,vil_cfg);
        }

        graph::Primitive *CreatePrimitive(  const AnsiString &name,
                                            const uint32_t vertices_count,
                                            const graph::VIL *vil,
                                            const std::initializer_list<graph::VertexAttribDataPtr> &vad_list);

        graph::Mesh *CreateMesh(const AnsiString &name,
                                            const uint32_t vertices_count,
                                            graph::MaterialInstance *mi,
                                            graph::Pipeline *pipeline,
                                            const std::initializer_list<graph::VertexAttribDataPtr> &vad_list);
    };//class WorkObject

    /**
    * 但我们认为游戏开发者不应该关注如何控制渲染，而应该关注如何处理游戏逻辑.
    * 所以我们在WorkObject的基础上再提供RenderWorkObject派生类，用于直接封装好的渲染场景树控制。
    * 
    * 开发者仅需要将要渲染的物件放置于场景树即可。

    * 但开发者也可以直接使用WorkObject，自行管理这些事。
    * */
}//namespcae hgl
