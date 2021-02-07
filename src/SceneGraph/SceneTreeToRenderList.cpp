#include<hgl/graph/RenderList.h>

namespace hgl
{
    namespace graph
    {
        SceneTreeToRenderList::~SceneTreeToRenderList()
        {
            SAFE_CLEAR(scene_node_list);
        }

        uint32 SceneTreeToRenderList::CameraLength(SceneNode *,SceneNode *)
        {
        }

        bool SceneTreeToRenderList::InFrustum(const SceneNode *,void *)
        {
        }

        bool SceneTreeToRenderList::Begin()
        {
            if(!scene_node_list)
                scene_node_list=new SceneNodeList;

            scene_node_list->ClearData();

            pipeline_sets.ClearData();
            material_sets.ClearData();
            mat_instance_sets.ClearData();

            return(true);
        }

        /**
        * 理论上讲，我们需要按以下顺序排序
        *
        * for(material)
        *   for(pipeline)
        *       for(material_instance)
        *           for(vbo)
        */

        bool SceneTreeToRenderList::End()
        {
        }

        bool SceneTreeToRenderList::Expend(SceneNode *sn)
        {
            if(!sn)return(false);

            if(sn->renderable_instances)
                scene_node_list->Add(sn);

            for(SceneNode *sub:sn->SubNode)
                Expend(sub);

            return(true);
        }

        bool SceneTreeToRenderList::Expend(RenderList *rl,Camera *c,SceneNode *sn)
        {
            if(!device)return(false);
            if(!rl||!c||sn)return(false);

            camera=c;
            camera->Refresh();
            camera_matrix=&(camera->matrix);

            //Frustum f;

            render_list=rl;

            Begin();
            Expend(sn);
            End();
        }
    }//namespace graph
}//namespace hgl
