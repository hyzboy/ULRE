#pragma once
#include<hgl/graph/PipelineMaterialBatch.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/type/UnorderedMap.h>
#include<vector>

VK_NAMESPACE_BEGIN
class Primitive;                    // fwd
class Material;                // fwd
class MaterialInstance;        // fwd
class Pipeline;                // fwd
class DrawNode;                // fwd
class NodeTransform;           // fwd

class RenderBatchMap:public hgl::UnorderedMap<PipelineMaterialIndex,PipelineMaterialBatch *>
{
    VulkanDevice *device = nullptr;                // 设备在构造/初始化时传入，供后续创建 batch 使用
    const CameraInfo *current_camera_info = nullptr;    // 记录 Begin 传入的相机信息，便于之后新建的 batch 同步设置

public:

    using hgl::UnorderedMap<PipelineMaterialIndex,PipelineMaterialBatch *>::Clear;
    using hgl::UnorderedMap<PipelineMaterialIndex,PipelineMaterialBatch *>::Get;
    using hgl::UnorderedMap<PipelineMaterialIndex,PipelineMaterialBatch *>::Add;

    void ClearAll(){ this->Clear(); }
    bool GetBatch(const PipelineMaterialIndex &key,PipelineMaterialBatch *&out){ return this->Get(key,out); }

    void Clear(){ hgl::UnorderedMap<PipelineMaterialIndex,PipelineMaterialBatch *>::Clear(); }
    bool Get(const PipelineMaterialIndex &key,PipelineMaterialBatch *&out){ return hgl::UnorderedMap<PipelineMaterialIndex,PipelineMaterialBatch *>::Get(key,out); }

    RenderBatchMap(VulkanDevice *dev=nullptr):device(dev){}
    virtual ~RenderBatchMap()=default;

    void SetDevice(VulkanDevice *dev){ device=dev; }

    void Begin(const CameraInfo *ci)
    {
        current_camera_info=ci;

        std::vector<PipelineMaterialBatch *> values;
        this->GetValueArray(values);
        for(auto *batch:values)
        {
            if(!batch)continue;
            batch->SetCameraInfo(ci);
            batch->Clear();
        }
    }

    void End()
    {
        std::vector<PipelineMaterialBatch *> values;
        this->GetValueArray(values);
        for(auto *batch:values)
        {
            if(batch)
                batch->Finalize();
        }
    }

    void Render(RenderCmdBuffer *rcb)
    {
        if(!rcb)return;

        std::vector<PipelineMaterialBatch *> values;
        this->GetValueArray(values);
        for(auto *batch:values)
        {
            if(batch)
                batch->Render(rcb);
        }
    }

    void UpdateTransformData()
    {
        std::vector<PipelineMaterialBatch *> values;
        this->GetValueArray(values);
        for(auto *batch:values)
        {
            if(batch)
                batch->UpdateTransformData();
        }
    }

    // 统一获取或创建 batch，并自动处理去重与设置相机
    PipelineMaterialBatch *GetOrCreate(const PipelineMaterialIndex &rpi)
    {
        PipelineMaterialBatch *mrl=nullptr;
        if(!this->Get(rpi,mrl))
        {
            mrl=new PipelineMaterialBatch(device,true,rpi);
            if(current_camera_info) mrl->SetCameraInfo(current_camera_info);
            this->Add(rpi,mrl);
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
        auto *primitive = node->GetPrimitive();
        auto *mi   = node->GetMaterialInstance();
        if(!primitive || !mi) { delete node; return; }
        auto *pl = primitive->GetPipeline();
        if(!pl) { delete node; return; }
        GetOrCreate(mi->GetMaterial(), pl)->Add(node);
    }
};//class RenderBatchMap
VK_NAMESPACE_END
