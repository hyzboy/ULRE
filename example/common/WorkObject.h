#pragma once
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderTarget.h>
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

        virtual void Join(graph::RenderFramework *rf,graph::IRenderTarget *rt)
        {
            if(!rf)return;
            if(render_framework==rf)return;

            render_framework=rf;
            cur_render_target=rt;
            db=rf->GetRenderResource();
        }

        virtual void Render(double delta_time,graph::RenderCmdBuffer *cmd)=0;

        virtual void Render(double delta_time)
        {
            if(!cur_render_target)
            {
                //std::cerr<<"WorkObject::Render,cur_render_target=nullptr"<<std::endl;
                return;
            }

            //std::cout<<"WorkObject::Render begin, render_dirty="<<(render_dirty?"true":"false")<<std::endl;

            if(render_dirty)
            {
                graph::RenderCmdBuffer *cmd=cur_render_target->BeginRender();

                if(!cmd)
                {
                    //std::cerr<<"WorkObject::Render,cur_render_target->BeginRender()=nullptr"<<std::endl;
                    return;
                }

                Render(delta_time,cmd);

                cur_render_target->EndRender();
                cur_render_target->Submit();

                render_dirty=false;
            }

            //std::cout<<"WorkObject::Render End"<<std::endl;
        }
    };//class WorkObject
}//namespcae hgl
