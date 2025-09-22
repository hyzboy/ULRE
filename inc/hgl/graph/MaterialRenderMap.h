#pragma once
#include<hgl/graph/PipelineMaterialBatch.h>
#include<hgl/graph/mesh/Mesh.h>

VK_NAMESPACE_BEGIN
class Mesh;                    // fwd
class Material;                // fwd
class MaterialInstance;        // fwd
class Pipeline;                // fwd
class DrawNode;                // fwd
class NodeTransform;           // fwd

class MaterialRenderMap:public ObjectMap<PipelineMaterialIndex,PipelineMaterialBatch>
{
    VulkanDevice *device = nullptr;                // 设备在构造/初始化时传入，供后续创建 batch 使用
    const CameraInfo *current_camera_info = nullptr;    // 记录 Begin 传入的相机信息，便于之后新建的 batch 同步设置
public:

    MaterialRenderMap(VulkanDevice *dev=nullptr):device(dev){}
    virtual ~MaterialRenderMap()=default;

    void SetDevice(VulkanDevice *dev){ device=dev; }

    void Begin(const CameraInfo *ci)
    {
        current_camera_info=ci;

        for(auto *it:data_list)
        {
            it->value->SetCameraInfo(ci);
            it->value->Clear();
        }
    }

    void End()
    {
        for(auto *it:data_list)
            it->value->Finalize();
    }

    void Render(RenderCmdBuffer *rcb)
    {
        if(!rcb)return;

        for(auto *it:data_list)
            it->value->Render(rcb);
    }

    void UpdateTransformData()
    {
        for(auto *it:data_list)
            it->value->UpdateTransformData();
    }

    // 统一获取或创建 batch，并自动处理去重与设置相机
    PipelineMaterialBatch *GetOrCreate(const PipelineMaterialIndex &rpi)
    {
        PipelineMaterialBatch *mrl=nullptr;
        if(!Get(rpi,mrl))
        {
            mrl=new PipelineMaterialBatch(device,true,rpi);
            if(current_camera_info) mrl->SetCameraInfo(current_camera_info);
            Add(rpi,mrl);
        }
        return mrl;
    }

    PipelineMaterialBatch *GetOrCreate(Material *m,Pipeline *p)
    {
        return GetOrCreate(PipelineMaterialIndex(m,p));
    }

    // 统一添加 DrawNode：根据 node 内的信息分发到对应 batch
    void AddDrawNode(DrawNode *node)
    {
        if(!node) return;
        auto *mesh = node->GetMesh();
        auto *mi   = node->GetMaterialInstance();
        if(!mesh || !mi) { delete node; return; }
        auto *pl = mesh->GetPipeline();
        if(!pl) { delete node; return; }
        GetOrCreate(mi->GetMaterial(), pl)->Add(node);
    }
};//class MaterialRenderMap
VK_NAMESPACE_END
