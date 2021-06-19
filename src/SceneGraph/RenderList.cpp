#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/util/sort/Sort.h>

/**
* 理论上讲，我们需要按以下顺序排序
*
*   for(pipeline)
*       for(material_instance)
*           for(vbo)
*               for(distance)
*/

template<> 
int Comparator<RenderNodePointer>::compare(const RenderNodePointer &obj_one,const RenderNodePointer &obj_two) const
{
    int off;

    hgl::graph::RenderableInstance *ri_one=obj_one->ri;
    hgl::graph::RenderableInstance *ri_two=obj_two->ri;

    //比较管线
    {
        off=ri_one->GetPipeline()
           -ri_two->GetPipeline();

        if(off)
            return off;
    }

    //比较材质实例
    {
        for(int i =(int)hgl::graph::DescriptorSetsType::BEGIN_RANGE;
                i<=(int)hgl::graph::DescriptorSetsType::END_RANGE;
                i++)
        {
            off=ri_one->GetMP((hgl::graph::DescriptorSetsType)i)
               -ri_two->GetMP((hgl::graph::DescriptorSetsType)i);

            if(off)
                return off;
        }
    }

    //比较vbo+ebo
    {
        off=ri_one->GetBufferHash()
           -ri_two->GetBufferHash();

        if(off)
            return off;
    }

    //比较距离
    {
        const double dist=obj_one->distance_to_camera_square-
                          obj_two->distance_to_camera_square;

        //由于距离差距可能会小于1，但又返回int，所以需要做如此处理

        if(dist>0)return  1;else
        if(dist<0)return -1;
    }

    return 0;
}

namespace hgl
{
    namespace graph
    {
        RenderList::RenderList(GPUDevice *dev)
        {
            device          =dev;
            cmd_buf         =nullptr;
            
            hgl_zero(camera_info);

            mvp_array       =new MVPArrayBuffer(device,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            
            ubo_offset      =0;
            ubo_align       =0;

            last_pipeline   =nullptr;
            hgl_zero(last_mp);
            last_vbo        =0;
        }

        RenderList::~RenderList()
        {
            delete mvp_array;
        }        

        bool RenderList::Begin()
        {
            render_node_list.ClearData();
            mvp_array->Clear();
            ri_list.ClearData();

            material_sets.ClearData();

            return(true);
        }

        void RenderList::End()
        {
            if(render_node_list.GetCount()<=0)return;

            //排序
            Sort(render_node_list,&render_node_comparator);

            //产生MVP矩阵UBO数据
            {
                const uint32_t count=render_node_list.GetCount();

                {
                    //按当前总节点数量分配UBO
                    mvp_array->Alloc(count);
                    mvp_array->Clear();

                    ri_list.ClearData();
                    ri_list.SetCount(count);       
                }

                {
                    ubo_align=mvp_array->GetUnitSize();
                    
                    char *mp=(char *)(mvp_array->Map(0,count));
                    RenderableInstance **ri=ri_list.GetData();

                    for(RenderNode *node:render_node_list)                  //未来可能要在Expend处考虑做去重
                    {
                        memcpy(mp,&(node->matrix),sizeof(MVPMatrix));
                        mp+=ubo_align;

                        (*ri)=node->ri;
                        ++ri;
                    }
                }
            }

            //为所有的材质绑定
            for(Material *mtl:material_sets)
            {
                MaterialParameters *mp=mtl->GetMP(DescriptorSetsType::Renderable);

                if(mp)
                    mp->BindUBO("r_scene_info",mvp_array->GetBuffer(),false);
            }
        }

        bool RenderList::Expend(SceneNode *sn)
        {
            if(!sn)return(false);

            RenderableInstance *ri=sn->GetRI();

            if(ri)
            {
                RenderNode *rn=new RenderNode;

                rn->matrix.Set(sn->GetLocalToWorldMatrix(),camera_info.vp);

                rn->WorldCenter=sn->GetWorldCenter();

                rn->distance_to_camera_square=length_squared(rn->WorldCenter,camera_info.pos);
//                rn->distance_to_camera=sqrtf(rn->distance_to_camera_square);

                rn->ri=ri;

                render_node_list.Add(rn);

                material_sets.Add(ri->GetMaterial());
            }

            for(SceneNode *sub:sn->SubNode)
                Expend(sub);

            return(true);
        }

        bool RenderList::Expend(const CameraInfo &ci,SceneNode *sn)
        {
            if(!device|!sn)return(false);

            camera_info=ci;

            Begin();
            Expend(sn);
            End();

            return(true);
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

            if(ri_list.GetCount()<=0)
                return(true);

            cmd_buf=cb;

            last_pipeline=nullptr;            
            hgl_zero(last_mp);
            last_vbo=0;
            ubo_offset=0;

            for(RenderableInstance *ri:ri_list)
                Render(ri);

            return(true);
        }
    }//namespace graph
}//namespace hgl
