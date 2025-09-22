#pragma once

#include<hgl/component/RenderComponent.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/MaterialRenderMap.h>

COMPONENT_NAMESPACE_BEGIN

class StaticMeshComponent;
class StaticMeshComponentManager;

struct StaticMeshComponentData:public ComponentData
{
    hgl::graph::StaticMesh *mesh=nullptr;

    StaticMeshComponentData()=default;
    explicit StaticMeshComponentData(hgl::graph::StaticMesh *m){mesh=m;}
    virtual ~StaticMeshComponentData(){mesh=nullptr;}

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
    WeakPtr<ComponentData> sm_data;

public:

    COMPONENT_CLASS_BODY(StaticMesh)

public:

    StaticMeshComponent(ComponentDataPtr cdp,StaticMeshComponentManager *cm):RenderComponent(cdp,cm)
    {
        sm_data=cdp;
    }

    virtual ~StaticMeshComponent()=default;

    virtual Component *Clone() override
    {
        StaticMeshComponent *smc=(StaticMeshComponent *)RenderComponent::Clone();
        if(!smc) return smc;
        return smc;
    }

            StaticMeshComponentData *GetData()      {return dynamic_cast<      StaticMeshComponentData *>(sm_data.get());}
    const   StaticMeshComponentData *GetData()const {return dynamic_cast<const StaticMeshComponentData *>(sm_data.const_get());}

    hgl::graph::StaticMesh *GetStaticMesh() const
    {
        if(!sm_data.valid())
            return nullptr;
        const StaticMeshComponentData *mcd=GetData();
        return mcd?mcd->mesh:nullptr;
    }

    const bool GetLocalAABB(AABB &box) const override
    {
        auto *sm=GetStaticMesh();
        if(!sm) return false;
        box=sm->GetBoundingBox();
        return true;
    }

    const bool CanRender() const override
    {
        const StaticMeshComponentData *mcd=GetData();
        if(!mcd||!mcd->mesh) return false;
        return mcd->mesh->GetSubMeshCount()>0;
    }

    // 由组件创建并提交 DrawNode（每个 submesh 一个节点）
    uint SubmitDrawNodes(hgl::graph::MaterialRenderMap &mrm) override
    {
        auto *sm = GetStaticMesh();
        if(!sm) return 0;
        uint submitted = 0;

        for(hgl::graph::Mesh *m : sm->GetSubMeshes())
        {
            auto *mi = m->GetMaterialInstance();
            auto *pl = m->GetPipeline();
            if(!mi||!pl) continue;

            mrm.AddDrawNode(new hgl::graph::OwnerMeshDrawNode(this, m));
            ++submitted;
        }
        return submitted;
    }
};

COMPONENT_NAMESPACE_END
