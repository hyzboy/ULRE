#include<hgl/graph/RenderList.h>
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
        RenderList::RenderList(GPUDevice *dev)
        {
            device          =dev;
            renderable_count=0;
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

            for(SceneNode *sub:sn->SubNode)
                ExpendNode(sub);

            return(true);
        }

        bool RenderList::Expend(SceneNode *sn)
        {
            if(!device|!sn)return(false);

            mrl_map.Begin();
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
    }//namespace graph
}//namespace hgl
