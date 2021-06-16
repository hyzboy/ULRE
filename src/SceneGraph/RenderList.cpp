#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKMaterialParameters.h>
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
            hgl_zero(last_mp);
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
                int ds_count=0;
                MaterialParameters *mp;

                for(int i=(int)DescriptorSetsType::BEGIN_RANGE;
                        i<(int)DescriptorSetsType::Renderable;
                        i++)
                {
                    mp=ri->GetMP((DescriptorSetsType)i);

                    if(last_mp[i]!=mp)
                    {
                        last_mp[i]=mp;

                        if(mp)
                        {
                            ds_list[ds_count]=mp->GetVkDescriptorSet();
                            ++ds_count;
                        }
                    }
                }

                {
                    mp=ri->GetMP(DescriptorSetsType::Renderable);

                    if(mp)
                    {
                        ds_list[ds_count]=mp->GetVkDescriptorSet();
                        ++ds_count;
                        
                        cmd_buf->BindDescriptorSets(ri->GetPipelineLayout(),ds_list,ds_count,&ubo_offset,1);
                    }
                    else
                    {                        
                        cmd_buf->BindDescriptorSets(ri->GetPipelineLayout(),ds_list,ds_count,nullptr,0);
                    }

                    ubo_offset+=ubo_align;
                }
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
            hgl_zero(last_mp);
            last_vbo=0;
            ubo_offset=0;

            for(RenderableInstance *ri:*ri_list)
                Render(ri);

            return(true);
        }
    }//namespace graph
}//namespace hgl
