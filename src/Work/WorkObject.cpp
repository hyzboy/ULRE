#include<hgl/WorkObject.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKRenderTargetSwapchain.h>
#include<hgl/Time.h>
//#include<iostream>

namespace hgl
{
    WorkObject::WorkObject(graph::RenderFramework *rf,graph::Renderer *r)
    {
        if(!r)
            renderer=rf->GetDefaultRenderer();

        OnRendererChange(rf,renderer);
    }

    void WorkObject::OnRendererChange(graph::RenderFramework *rf,graph::Renderer *r)
    {
        if(!rf)
        {
            render_framework=nullptr;
            db=nullptr;
        }

        if(!rf||!r)
        {
            return;
        }

        render_framework=rf;

        db=rf->GetRenderResource();
        scene=rf->GetDefaultScene();
        renderer=rf->GetDefaultRenderer();
    }

    void WorkObject::Render(double delta_time)
    {
        if(!renderer)
        {
            //std::cerr<<"WorkObject::Render,renderer=nullptr"<<std::endl;
            return;
        }

        //std::cout<<"WorkObject::Render begin, render_dirty="<<(render_dirty?"true":"false")<<std::endl;

        if(render_dirty)
        {
            renderer->RenderFrame();

            render_dirty=false;
        }

        renderer->Submit();

        //std::cout<<"WorkObject::Render End"<<std::endl;
    }

    graph::Primitive *WorkObject::CreatePrimitive(const AnsiString &name,
                                                  const uint32_t vertices_count,
                                                  const graph::VIL *vil,
                                                  const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
    {
        auto *pc=new graph::PrimitiveCreater(GetDevice(),vil);

        pc->Init(name,vertices_count);

        for(const auto &vad:vad_list)
        {
            if(!pc->WriteVAB(vad.name,vad.format,vad.data))
            {
                delete pc;
                return(nullptr);
            }
        }

        auto *prim=pc->Create();

        if(prim)
            db->Add(prim);

        return prim;
    }

    graph::Mesh *WorkObject::CreateMesh( const AnsiString &name,
                                                     const uint32_t vertices_count,
                                                     graph::MaterialInstance *mi,
                                                     graph::Pipeline *pipeline,
                                                     const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
    {
        auto *prim=this->CreatePrimitive(name,vertices_count,mi->GetVIL(),vad_list);

        if(!prim)
            return(nullptr);

        return db->CreateMesh(prim,mi,pipeline);
    }
}//namespcae hgl
