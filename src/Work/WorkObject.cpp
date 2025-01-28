#include<hgl/WorkObject.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/PrimitiveCreater.h>
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
        render_pass=rt->GetRenderPass();

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

    graph::Renderable *WorkObject::CreateRenderable( const AnsiString &name,
                                                     uint32_t vertices_count,
                                                     graph::MaterialInstance *mi,
                                                     graph::Pipeline *pipeline,
                                                     const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
    {
        auto *pc=new graph::PrimitiveCreater(GetDevice(),mi->GetVIL());

        pc->Init(name,vertices_count);

        for(const auto &vad:vad_list)
        {
            if(!pc->WriteVAB(vad.name,vad.format,vad.data))
            {
                delete pc;
                return(nullptr);
            }
        }

        auto *result=db->CreateRenderable(pc,mi,pipeline);

        delete pc;
        return result;
    }
}//namespcae hgl
