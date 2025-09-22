#include<hgl/graph/PipelineMaterialBatch.h>
#include<hgl/graph/RenderCollector.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/mesh/Mesh.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/component/StaticMeshComponent.h>

namespace hgl
{
    namespace graph
    {
        RenderCollector::RenderCollector(VulkanDevice *dev)
        {
            device          =dev;
            renderable_count=0;

            camera_info=nullptr;

            mrl_map.SetDevice(device);
        }

        bool RenderCollector::ExpandNode(SceneNode *sn)
        {
            if(!sn)return(false);

            for(auto component:sn->GetComponents())
            {
                auto *rc = dynamic_cast<COMPONENT_NAMESPACE::RenderComponent *>(component);
                if(!rc) continue;
                if(!rc->CanRender()) continue;

                renderable_count += rc->SubmitDrawNodes(mrl_map);
            }

            for(SceneNode *sub:sn->GetChildNode())
                ExpandNode(sub);

            return(true);
        }

        uint RenderCollector::Expand(SceneNode *sn)
        {
            if(!device||!sn) return 0;

            renderable_count = 0;        // reset per build
            mrl_map.Begin(camera_info);
            ExpandNode(sn);
            mrl_map.End();

            return renderable_count;
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
        
        void RenderCollector::UpdateTransformData()
        {
            if(renderable_count<=0)
                return;

            mrl_map.UpdateTransformData();
        }

        void RenderCollector::UpdateMaterialInstanceData(MeshComponent *smc)
        {
            if(!smc)return;

            if(!smc->CanRender())return;

            PipelineMaterialIndex rli(smc->GetMaterial(),smc->GetPipeline());
            PipelineMaterialBatch *mrl;

            if(!mrl_map.Get(rli,mrl))        //找到对应的
                return;

            mrl->UpdateMaterialInstanceData(smc);
        }
    }//namespace graph
}//namespace hgl
