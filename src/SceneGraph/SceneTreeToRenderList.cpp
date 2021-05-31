#include<hgl/graph/SceneTreeToRenderList.h>

namespace hgl
{
    namespace graph
    {    
        SceneTreeToRenderList::SceneTreeToRenderList(GPUDevice *d)
        {
            device=d;
            hgl_zero(camera_info);

            scene_node_list=nullptr;
            render_list=nullptr;
        }

        SceneTreeToRenderList::~SceneTreeToRenderList()
        {
            SAFE_CLEAR(scene_node_list);
        }

        float SceneTreeToRenderList::CameraLength(SceneNode *obj_one,SceneNode *obj_two)
        {
            if(!obj_one||!obj_two)
                return(0);

            return( length_squared(obj_one->GetCenter(),camera_info.pos)-
                    length_squared(obj_two->GetCenter(),camera_info.pos));
        }

        //bool SceneTreeToRenderList::InFrustum(const SceneNode *,void *)
        //{
        //    return(true);
        //}

        bool SceneTreeToRenderList::Begin()
        {
            if(!scene_node_list)
                scene_node_list=new SceneNodeList;

            scene_node_list->ClearData();

            pipeline_sets.ClearData();
            material_sets.ClearData();
            mat_inst_sets.ClearData();

            return(true);
        }

        /**
        * 理论上讲，我们需要按以下顺序排序
        *
        * for(material)
        *   for(pipeline)
        *       for(material_instance)
        *           for(vbo)
        *               for(distance)
        */

        bool SceneTreeToRenderList::End()
        {
        }

        bool SceneTreeToRenderList::Expend(SceneNode *sn)
        {
            if(!sn)return(false);

            if(sn->RIList.GetCount()>0)
                scene_node_list->Add(sn);

            for(SceneNode *sub:sn->SubNode)
                Expend(sub);

            return(true);
        }

        bool SceneTreeToRenderList::Expend(RenderList *rl,const CameraInfo &ci,SceneNode *sn)
        {
            if(!device)return(false);
            if(!rl||sn)return(false);

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
