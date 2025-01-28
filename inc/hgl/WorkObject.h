#pragma once
#include<hgl/graph/RenderFramework.h>
#include<hgl/type/object/TickObject.h>
#include<hgl/Time.h>
//#include<iostream>

namespace hgl
{
    /**
    * 工作对象</p>
    */
    class WorkObject:public TickObject
    {
        graph::RenderFramework *render_framework=nullptr;
        graph::IRenderTarget *cur_render_target=nullptr;

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

        WorkObject()=default;
        virtual ~WorkObject()=default;

        virtual void Join(graph::RenderFramework *rf,graph::IRenderTarget *rt);

        virtual void Render(double delta_time,graph::RenderCmdBuffer *cmd)=0;

        virtual void Render(double delta_time);
    };//class WorkObject
}//namespcae hgl
