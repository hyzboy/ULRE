#include<hgl/graph/MaterialRenderList.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKRenderable.h>

namespace hgl
{
    namespace graph
    {
        RenderList::RenderList(GPUDevice *dev)
        {
            device          =dev;
            renderable_count=0;

            camera_info=nullptr;
        }

        bool RenderList::ExpendNode(SceneNode *sn)
        {
            if(!sn)return(false);

            Renderable *ri=sn->GetRenderable();

            if(ri)
            {
                Material *mtl=ri->GetMaterial();
                MaterialRenderList *mrl;

                if(!mrl_map.Get(mtl,mrl))
                {
                    mrl=new MaterialRenderList(device,true,mtl);

                    mrl_map.Add(mtl,mrl);
                }
                
                mrl->Add(sn);

                ++renderable_count;
            }

            for(SceneNode *sub:sn->GetSubNode())
                ExpendNode(sub);

            return(true);
        }

        bool RenderList::Expend(SceneNode *sn)
        {
            if(!device|!sn)return(false);

            mrl_map.Begin(camera_info);
            ExpendNode(sn);
            mrl_map.End();

            return(true);
        }

        bool RenderList::Render(RenderCmdBuffer *cb) 
        {
            if(!cb)
                return(false);

            if(renderable_count<=0)
                return(true);

            mrl_map.Render(cb);

            return(true);
        }

        void RenderList::Clear()
        {
            mrl_map.Clear();
        }
        
        void RenderList::UpdateLocalToWorld()
        {
            if(renderable_count<=0)
                return;

            mrl_map.UpdateLocalToWorld();
        }

        void RenderList::UpdateMaterialInstance(SceneNode *sn)
        {
            if(!sn)return;

            Renderable *ri=sn->GetRenderable();

            if(!ri)return;

            Material *mtl=ri->GetMaterial();
            MaterialRenderList *mrl;

            if(!mrl_map.Get(mtl,mrl))        //找到对应的
                return;

            mrl->UpdateMaterialInstance(sn);
        }
    }//namespace graph
}//namespace hgl
