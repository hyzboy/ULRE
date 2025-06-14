#include<hgl/graph/MaterialRenderList.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/Mesh.h>
#include<hgl/component/StaticMeshComponent.h>

namespace hgl
{
    namespace graph
    {
        RenderList::RenderList(VulkanDevice *dev)
        {
            device          =dev;
            renderable_count=0;

            camera_info=nullptr;
        }

        bool RenderList::ExpendNode(SceneNode *sn)
        {
            if(!sn)return(false);

            for(auto component:sn->GetComponents())
            {
                if(component->GetHashCode()!=StaticMeshComponent::StaticHashCode())     //暂时只支持StaticMeshComponent
                    continue;

                StaticMeshComponent *smc=reinterpret_cast<StaticMeshComponent *>(component);

                Mesh *mesh=smc->GetMesh();

                RenderPipelineIndex rpi(mesh->GetMaterial(),mesh->GetPipeline());
                
                MaterialRenderList *mrl;

                if(!mrl_map.Get(rpi,mrl))
                {
                    mrl=new MaterialRenderList(device,true,rpi);

                    mrl_map.Add(rpi,mrl);
                }

                mrl->Add(smc);

                ++renderable_count;
            }

            for(SceneNode *sub:sn->GetChildNode())
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

        void RenderList::UpdateMaterialInstance(StaticMeshComponent *smc)
        {
            if(!smc)return;

            Mesh *ri=smc->GetMesh();

            if(!ri)return;

            RenderPipelineIndex rli(ri->GetMaterial(),ri->GetPipeline());
            MaterialRenderList *mrl;

            if(!mrl_map.Get(rli,mrl))        //找到对应的
                return;

            mrl->UpdateMaterialInstance(smc);
        }
    }//namespace graph
}//namespace hgl
