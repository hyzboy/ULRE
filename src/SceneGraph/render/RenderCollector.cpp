#include<hgl/graph/MaterialRenderList.h>
#include<hgl/graph/RenderCollector.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/Mesh.h>
#include<hgl/component/MeshComponent.h>

namespace hgl
{
    namespace graph
    {
        RenderCollector::RenderCollector(VulkanDevice *dev)
        {
            device          =dev;
            renderable_count=0;

            camera_info=nullptr;
        }

        bool RenderCollector::ExpendNode(SceneNode *sn)
        {
            if(!sn)return(false);

            for(auto component:sn->GetComponents())
            {
                if(component->GetTypeHash()!=MeshComponent::StaticTypeHash())     //暂时只支持MeshComponent
                    continue;

                MeshComponent *smc=(MeshComponent *)component;

                if(!smc||!smc->CanRender())
                    continue;

                RenderPipelineIndex rpi(smc->GetMaterial(),smc->GetPipeline());
                
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

        bool RenderCollector::Expend(SceneNode *sn)
        {
            if(!device|!sn)return(false);

            mrl_map.Begin(camera_info);
            ExpendNode(sn);
            mrl_map.End();

            return(true);
        }

        bool RenderCollector::Render(RenderCmdBuffer *cb) 
        {
            if(!cb)
                return(false);

            if(renderable_count<=0)
                return(true);

            mrl_map.Render(cb);

            return(true);
        }

        void RenderCollector::Clear()
        {
            mrl_map.Clear();
        }
        
        void RenderCollector::UpdateLocalToWorld()
        {
            if(renderable_count<=0)
                return;

            mrl_map.UpdateLocalToWorld();
        }

        void RenderCollector::UpdateMaterialInstance(MeshComponent *smc)
        {
            if(!smc)return;

            if(!smc->CanRender())return;

            RenderPipelineIndex rli(smc->GetMaterial(),smc->GetPipeline());
            MaterialRenderList *mrl;

            if(!mrl_map.Get(rli,mrl))        //找到对应的
                return;

            mrl->UpdateMaterialInstance(smc);
        }
    }//namespace graph
}//namespace hgl
