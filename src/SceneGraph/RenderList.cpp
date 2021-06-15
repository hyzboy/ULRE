#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKRenderableInstance.h>

namespace hgl
{
    namespace graph
    {
        RenderList::RenderList()
        {
            cmd_buf         =nullptr;

            mvp_buffer      =nullptr;
            ri_list         =nullptr;

            ubo_offset      =0;
            ubo_align       =0;

            last_pipeline   =nullptr;
            last_mi         =nullptr;
            last_vbo        =0;
        }

        void RenderList::Set(List<RenderableInstance *> *ril,GPUBuffer *buf,const uint32_t align)
        {
            ri_list=ril;
            mvp_buffer=buf;
            ubo_align=align;
        }

        void RenderList::Render(RenderableInstance *ri)
        {
            if(last_pipeline!=ri->GetPipeline())
            {
                last_pipeline=ri->GetPipeline();

                cmd_buf->BindPipeline(last_pipeline);
            }

            {
                MaterialInstance *mi=ri->GetMaterialInstance();

                if(mi!=last_mi)
                {
                    last_mi=mi;
                    cmd_buf->BindDescriptorSets(mi->GetDescriptorSets());
                }
            }

            {
                
            }

            if(last_vbo!=ri->GetBufferHash())
            {
                last_vbo=ri->GetBufferHash();
                
                cmd_buf->BindVAB(ri);
            }

            const IndexBuffer *ib=ri->GetIndexBuffer();

            if(ib)
            {
                cmd_buf->DrawIndexed(ib->GetCount());
            }
            else
            {
                cmd_buf->Draw(ri->GetDrawCount());
            }
        }

        bool RenderList::Render(RenderCmdBuffer *cb) 
        {
            if(!cb)
                return(false);

            if(!mvp_buffer
             ||!ri_list)
                return(false);

            if(ri_list->GetCount()<=0)
                return(true);

            cmd_buf=cb;

            last_pipeline=nullptr;
            last_mi=nullptr;
            last_vbo=0;
            ubo_offset=0;

            for(RenderableInstance *ri:*ri_list)
                Render(ri);

            return(true);
        }
    }//namespace graph
}//namespace hgl
