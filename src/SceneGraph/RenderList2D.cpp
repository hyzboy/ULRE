#include<hgl/graph/RenderList2D.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/util/sort/Sort.h>

namespace hgl
{
    namespace graph
    {
        RenderList2D::RenderList2D(GPUDevice *dev)
        {
            device          =dev;
            cmd_buf         =nullptr;

            last_pipeline   =nullptr;
            hgl_zero(last_mp);
            last_vbo        =0;
        }

        RenderList2D::~RenderList2D()
        {
        }

        bool RenderList2D::Begin()
        {
            renderable_count=0;
            mrl_map.Begin();

            return(true);
        }

        void RenderList2D::End()
        {
            if(renderable_count<=0)return;
            
            mrl_map.End();
            
            
//
//            //排序
//            Sort(render_node_list,&render_node_comparator);
//
//            //产生MVP矩阵UBO数据
//            {
//                const uint32_t count=render_node_list.GetCount();
//
//                {
//                    //按当前总节点数量分配UBO
////                    mvp_array->Alloc(count);
////                    mvp_array->Clear();
//
//                    ri_list.ClearData();
//                    ri_list.SetCount(count);
//                }
//
//                {
////                    ubo_align=mvp_array->GetAlignSize();
//                    
////                    char *mp=(char *)(mvp_array->Map(0,count));
//                    Renderable **ri=ri_list.GetData();
//
//                    for(RenderNode2D *node:render_node_list)                  //未来可能要在Expend处考虑做去重
//                    {
////                        memcpy(mp,&(node->matrix),MVPMatrixBytes);
////                        mp+=ubo_align;
//
//                        (*ri)=node->ri;
//                        ++ri;
//                    }
//
////                    mvp_array->Flush(count);
//                }
//            }
//
//            //为所有的材质绑定
//            //for(Material *mtl:material_sets)
//            //{
//            //    MaterialParameters *mp=mtl->GetMP(DescriptorSetType::PerObject);
//
//            //    if(mp)
//            //    {
//            //        if(mp->BindUBO("r_scene_info",mvp_array->GetBuffer(),true))
//            //            mp->Update();
//            //    }
//            //}
        }

        bool RenderList2D::ExpendNode(SceneNode *sn)
        {
            if(!sn)return(false);

            Renderable *ri=sn->GetRenderable();

            if(ri)
            {
                Material *mtl=ri->GetMaterial();
                MaterialRenderList2D *mrl;

                if(!mrl_map.Get(mtl,mrl))
                {
                    mrl=new MaterialRenderList2D(mtl);

                    mrl_map.Add(mtl,mrl);
                }
                
                mrl->Add(ri,sn->GetLocalToWorldMatrix());

                ++renderable_count;
            }

            for(SceneNode *sub:sn->SubNode)
                Expend(sub);

            return(true);
        }

        bool RenderList2D::Expend(SceneNode *sn)
        {
            if(!device|!sn)return(false);

            Begin();
            ExpendNode(sn);
//            End();

            return(true);
        }

        //bool RenderList2D::BindPerFrameDescriptor()
        //{
        //    if(!cmd_buf)return(false);

        //    for(Material *mtl:material_sets)
        //    {
        //        MaterialParameters *mp=mtl->GetMP(DescriptorSetType::PerFrame);

        //        if(!mp)continue;
        //        
        //        //if(mp->BindUBO("r_scene_info",mvp_array->GetBuffer(),true))
        //          //  mp->Update();
        //    }

        //    return(true);
        //}

        //bool RenderList2D::BindPerMaterialDescriptor()
        //{
        //    //为每个材质实例，绑定它们的描述符
        //    if(!cmd_buf)return(false);

        //    for(Material *mtl:material_sets)
        //    {
        //        MaterialParameters *mp=mtl->GetMP(DescriptorSetType::PerMaterial);

        //        if(!mp)continue;
        //        
        //        //if(mp->BindUBO("r_scene_info",mvp_array->GetBuffer(),true))
        //        //    mp->Update();
        //    }

        //    return(true);
        //}

        void RenderList2D::Render(Renderable *ri)
        {
            if(last_pipeline!=ri->GetPipeline())
            {
                last_pipeline=ri->GetPipeline();

                cmd_buf->BindPipeline(last_pipeline);
            }

            {
                uint32_t ds_count=0;
                uint32_t first_set=0;
                MaterialParameters *mp;

                ENUM_CLASS_FOR(DescriptorSetType,int,i)
                {
                    if(i==(int)DescriptorSetType::PerObject)continue;

                    mp=ri->GetMP((DescriptorSetType)i);

                    if(last_mp[i]!=mp)
                    {
                        last_mp[i]=mp;

                        if(mp)
                        {
                            ds_list[ds_count]=mp->GetVkDescriptorSet();
                            ++ds_count;
                        }
                    }
                    else
                    {
                        if(mp)
                            ++first_set;
                    }
                }

                {
                    mp=ri->GetMP(DescriptorSetType::PerObject);

                    if(mp)
                    {
                        ds_list[ds_count]=mp->GetVkDescriptorSet();
                        ++ds_count;

                        cmd_buf->BindDescriptorSets(ri->GetPipelineLayout(),first_set,ds_list,ds_count,&ubo_offset,1);
                    }
                    else
                    {
                        cmd_buf->BindDescriptorSets(ri->GetPipelineLayout(),first_set,ds_list,ds_count,nullptr,0);
                    }

                    ubo_offset+=ubo_align;
                }
            }

            if(last_vbo!=ri->GetBufferHash())
            {
                last_vbo=ri->GetBufferHash();
                
                cmd_buf->BindVBO(ri);
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

        bool RenderList2D::Render(RenderCmdBuffer *cb) 
        {
            if(!cb)
                return(false);

            if(ri_list.GetCount()<=0)
                return(true);

            cmd_buf=cb;

            last_mtl=nullptr;
            last_pipeline=nullptr;
            hgl_zero(last_mp);
            last_vbo=0;
            ubo_offset=0;

            BindPerFrameDescriptor();

            for(Renderable *ri:ri_list)
                Render(ri);

            return(true);
        }
    }//namespace graph
}//namespace hgl
