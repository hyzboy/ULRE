#include<hgl/graph/RenderList.h>

namespace hgl
{
    namespace graph
    {
        uint32 SceneTreeToRenderList::CameraLength(SceneNode *,SceneNode *)
        {
        }

        bool SceneTreeToRenderList::InFrustum(const SceneNode *,void *)
        {
        }

        bool SceneTreeToRenderList::Expend(RenderList *rl,Camera *c,SceneNode *sn)
        {
            if(!device)return(false);
            if(!rl||!c||sn)return(false);

            camera=c;
            camera->Refresh();
            camera_matrix=&(camera->matrix);

            //Frustum f;


        }
    }//namespace graph
}//namespace hgl
