#include<hgl/WorkObject.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/type/object/TickObject.h>
#include<hgl/Time.h>
//#include<iostream>

namespace hgl
{
    void WorkObject::Join(graph::RenderFramework *rf,graph::IRenderTarget *rt)
    {
        if(!rf)return;
        if(render_framework==rf)return;

        render_framework=rf;
        cur_render_target=rt;
        db=rf->GetRenderResource();
    }

    void WorkObject::Render(double delta_time)
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
}//namespcae hgl
