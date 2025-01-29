#pragma once
#include<hgl/type/object/TickObject.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKRenderResource.h>
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
        graph::IRenderTarget *cur_render_target=nullptr;
        graph::RenderPass *render_pass=nullptr;

        bool destroy_flag=false;

        bool renderable=true;
        bool render_dirty=true;

    protected:

        graph::RenderResource *db=nullptr;          //暂时的，未来会被更好的机制替代

    public:

        graph::RenderFramework *    GetRenderFramework  (){return render_framework;}
        graph::GPUDevice *          GetDevice           (){return render_framework->GetDevice();}
        graph::GPUDeviceAttribute * GetDeviceAttribute  (){return render_framework->GetDeviceAttribute();}

    public:

        const bool IsDestroy()const{return destroy_flag;}
        const bool IsRenderable()const{return renderable;}
        const bool IsRenderDirty()const{return render_dirty;}

        void MarkDestory(){destroy_flag=true;}
        void SetRenderable(bool r){renderable=r;}
        void MarkRenderDirty(){render_dirty=true;}

    public:

        WorkObject(graph::RenderFramework *,graph::IRenderTarget *);
        virtual ~WorkObject()=default;

        virtual void OnRenderTargetSwitch(graph::RenderFramework *rf,graph::IRenderTarget *rt);

        virtual void Render(double delta_time,graph::RenderCmdBuffer *cmd)=0;

        virtual void Render(double delta_time);

    public:

        template<typename ...ARGS>
        graph::Pipeline *CreatePipeline(ARGS...args)
        {
            return render_pass->CreatePipeline(args...);
        }

        graph::MaterialInstance *CreateMaterialInstance(const graph::mtl::MaterialCreateInfo *mci,const graph::VILConfig *vil_cfg=nullptr)
        {
            return db->CreateMaterialInstance(mci,vil_cfg);
        }

        graph::Renderable *CreateRenderable( const AnsiString &name,
                                             uint32_t vertices_count,
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
