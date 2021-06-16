#include<hgl/graph/SceneTreeToRenderList.h>
#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/util/sort/Sort.h>

/**
* 理论上讲，我们需要按以下顺序排序
*
* for(material)
*   for(pipeline)
*       for(material_instance)
*           for(vbo)
*               for(distance)
*/

template<> 
int Comparator<RenderNodePointer>::compare(const RenderNodePointer &obj_one,const RenderNodePointer &obj_two) const
{
    int off;

    //比较材质
    hgl::graph::MaterialParameters *mi1=obj_one->ri->GetMaterialInstance();
    hgl::graph::MaterialParameters *mi2=obj_two->ri->GetMaterialInstance();
                       
    off=mi1->GetMaterial()-mi2->GetMaterial();

    if(off)
        return off;
            
    //比较管线
    hgl::graph::Pipeline *p1=obj_one->ri->GetPipeline();
    hgl::graph::Pipeline *p2=obj_two->ri->GetPipeline();

    off=p1-p2;

    if(off)
        return off;

    //比较材质实例
    off=mi1-mi2;

    if(off)
        return off;
            
    //比较vbo+ebo
    off=obj_one->ri->GetBufferHash()
       -obj_two->ri->GetBufferHash();

    if(off)
        return off;

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
        SceneTreeToRenderList::SceneTreeToRenderList(GPUDevice *d)
        {
            device=d;
            hgl_zero(camera_info);
            
            mvp_array   =new MVPArrayBuffer(device,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            render_list =nullptr;
        }

        SceneTreeToRenderList::~SceneTreeToRenderList()
        {
            delete mvp_array;
        }

        bool SceneTreeToRenderList::Begin()
        {
            render_node_list.ClearData();
            mvp_array->Clear();
            ri_list.ClearData();

            //pipeline_sets.ClearData();
            //material_sets.ClearData();
            //mat_inst_sets.ClearData();

            return(true);
        }

        void SceneTreeToRenderList::End()
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

                    ri_list.SetCount(count);
                }

                {
                    const uint32_t unit_offset=mvp_array->GetUnitSize();
                    
                    char *mp=(char *)(mvp_array->Map(0,count));
                    RenderableInstance **ri=ri_list.GetData();

                    for(RenderNode *node:render_node_list)                  //未来可能要在Expend处考虑做去重
                    {
                        memcpy(mp,&(node->matrix),sizeof(MVPMatrix));
                        mp+=unit_offset;

                        (*ri)=node->ri;
                        ++ri;
                    }
                }
            }

            //写入RenderList
            render_list->Set(   &ri_list,
                                mvp_array->GetBuffer(),
                                mvp_array->GetUnitSize());
        }

        bool SceneTreeToRenderList::Expend(SceneNode *sn)
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
            }

            for(SceneNode *sub:sn->SubNode)
                Expend(sub);

            return(true);
        }

        bool SceneTreeToRenderList::Expend(RenderList *rl,const CameraInfo &ci,SceneNode *sn)
        {
            if(!device)return(false);
            if(!rl||!sn)return(false);

            camera_info=ci;

            //Frustum f;

            render_list=rl;

            Begin();
            Expend(sn);
            End();

            return(true);
        }
    }//namespace graph
}//namespace hgl
