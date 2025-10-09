#pragma once

#include<hgl/component/RenderComponent.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/RenderBatchMap.h>
#include<hgl/graph/mesh/MeshNode.h>
#include<hgl/graph/DrawNode.h>

COMPONENT_NAMESPACE_BEGIN

class StaticMeshComponent;
class StaticMeshComponentManager;

struct StaticMeshComponentData:public ComponentData
{
    hgl::graph::StaticMesh *primitive=nullptr;

    StaticMeshComponentData()=default;
    explicit StaticMeshComponentData(hgl::graph::StaticMesh *m){primitive=m;}
    virtual ~StaticMeshComponentData(){primitive=nullptr;}

    COMPONENT_DATA_CLASS_BODY(StaticMesh)
};

class StaticMeshComponentManager:public ComponentManager
{
public:

    COMPONENT_MANAGER_CLASS_BODY(StaticMesh)

public:

    StaticMeshComponentManager()=default;

    Component *CreateComponent(ComponentDataPtr cdp) override;
    Component *CreateComponent(hgl::graph::StaticMesh *);
};

class StaticMeshComponent:public RenderComponent
{
    WeakPtr<ComponentData> primitive_data;

public:

    COMPONENT_CLASS_BODY(StaticMesh)

public:

    StaticMeshComponent(ComponentDataPtr cdp,StaticMeshComponentManager *cm):RenderComponent(cdp,cm)
    {
        primitive_data=cdp;
    }

    virtual ~StaticMeshComponent()=default;

    virtual Component *Clone() override
    {
        StaticMeshComponent *smc=(StaticMeshComponent *)RenderComponent::Clone();
        if(!smc) return smc;
        return smc;
    }

            StaticMeshComponentData *GetData()      {return dynamic_cast<      StaticMeshComponentData *>(primitive_data.get());}
    const   StaticMeshComponentData *GetData()const {return dynamic_cast<const StaticMeshComponentData *>(primitive_data.const_get());}

    hgl::graph::StaticMesh *GetStaticMesh() const
    {
        if(!primitive_data.valid())
            return nullptr;
        const StaticMeshComponentData *mcd=GetData();
        return mcd?mcd->primitive:nullptr;
    }

    const bool GetLocalAABB(AABB &box) const override
    {
        auto *sm=GetStaticMesh();
        if(!sm) return false;

        box=sm->GetBoundingVolumes().aabb;
        return true;
    }

    const bool CanRender() const override
    {
        const StaticMeshComponentData *mcd=GetData();
        if(!mcd||!mcd->primitive) return false;
        return mcd->primitive->GetPrimitiveCount()>0;
    }

    // 由组件创建并提交 DrawNode（按 MeshNode * Primitive 组合提交），叠加 MeshNode 变换
    uint SubmitDrawNodes(hgl::graph::RenderBatchMap &mrm) override
    {
        auto *sm = GetStaticMesh();
        if(!sm) return 0;
        uint submitted = 0;

        for(hgl::graph::MeshNode *mn : sm->GetNodes())
        {
            for(hgl::graph::Primitive *m : mn->GetPrimitiveSet())
            {
                auto *mi = m->GetMaterialInstance();
                auto *pl = m->GetPipeline();
                if(!mi||!pl) continue;

                mrm.AddDrawNode(new hgl::graph::DrawNodeStaticMesh(this, mn, m));
                ++submitted;
            }
        }
        return submitted;
    }
};

COMPONENT_NAMESPACE_END
