#pragma once
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/type/object/TickObject.h>
#include<hgl/Time.h>

namespace hgl
{
    class WorkObject:public TickObject
    {
        graph::RenderFramework *render_framework=nullptr;

        bool destroy_flag=false;

        bool renderable=true;

    protected:

        graph::RenderResource *db=nullptr;          //暂时的，未来会被更好的机制替代

    public:

        graph::RenderFramework *    GetRenderFramework  (){return render_framework;}
        graph::GPUDevice *          GetDevice           (){return render_framework->GetDevice();}
        graph::GPUDeviceAttribute * GetDeviceAttribute  (){return render_framework->GetDeviceAttribute();}

    public:

        const bool IsDestroy()const{return destroy_flag;}
        const bool IsRenderable()const{return renderable;}

        void MarkDestory(){destroy_flag=true;}
        void SetRenderable(bool r){renderable=r;}

    public:

        WorkObject(graph::RenderFramework *rf)
        {
            Join(rf);
        }
        virtual ~WorkObject()=default;

        virtual void Join(graph::RenderFramework *rf)
        {
            if(!rf)return;
            if(render_framework==rf)return;

            render_framework=rf;
            db=rf->GetRenderResource();
        }

        virtual void Render(double delta_time)=0;
    };//class WorkObject
}//namespcae hgl
