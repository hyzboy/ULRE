#pragma once

#include<hgl/component/RenderComponent.h>
#include<hgl/graph/mesh/Mesh.h>
#include<hgl/graph/MaterialRenderMap.h>

COMPONENT_NAMESPACE_BEGIN

class MeshComponent;
class MeshComponentManager;

struct MeshComponentData:public ComponentData
{
    //static uint unique_id_count;

    //uint unique_id;

    Mesh *mesh;

public:

    MeshComponentData()
    {
        mesh=nullptr;

//        unique_id=++unique_id_count;
//        LogInfo(AnsiString("MeshComponentData():")+AnsiString::numberOf(unique_id));
    }

    MeshComponentData(Mesh *m)
    {
        mesh=m;

//        unique_id=++unique_id_count;
//        LogInfo(AnsiString("MeshComponentData(Mesh *):")+AnsiString::numberOf(unique_id));
    }

    virtual ~MeshComponentData();

    COMPONENT_DATA_CLASS_BODY(Mesh)
};//struct MeshComponentData

class MeshComponentManager:public ComponentManager
{
public:

    COMPONENT_MANAGER_CLASS_BODY(Mesh)

public:

    MeshComponentManager()=default;

    Component *CreateComponent(ComponentDataPtr cdp) override;

    Component *CreateComponent(Mesh *);
};//class MeshComponentManager

class MeshComponent:public RenderComponent
{
    WeakPtr<ComponentData> sm_data;
    MaterialInstance* override_material = nullptr; // 新增

public:

    COMPONENT_CLASS_BODY(Mesh)

public:

    MeshComponent(ComponentDataPtr cdp,MeshComponentManager *cm):RenderComponent(cdp,cm)
    {
        sm_data=cdp;
    }

    virtual ~MeshComponent()=default;

    virtual Component *Clone() override
    {
        MeshComponent *mc=(MeshComponent *)RenderComponent::Clone();

        if(!mc)
            return(mc);

        mc->override_material=override_material;
        return mc;
    }

            MeshComponentData *GetData()      {return dynamic_cast<      MeshComponentData *>(sm_data.get());}
    const   MeshComponentData *GetData()const {return dynamic_cast<const MeshComponentData *>(sm_data.const_get());}

    Mesh *GetMesh()const
    {
        if(!sm_data.valid())
            return(nullptr);

        const MeshComponentData *mcd=dynamic_cast<const MeshComponentData *>(sm_data.const_get());

        if(!mcd)
            return(nullptr);

        return mcd->mesh;
    }

    const bool GetLocalAABB(AABB &box) const override
    {
        Mesh *mesh=GetMesh();

        if (!mesh)
            return false;

        box=mesh->GetBoundingBox();
        return true;
    }

public:

    Pipeline *GetPipeline() const
    {
        Mesh *mesh=GetMesh();

        if (!mesh)
            return nullptr;

        return mesh->GetPipeline();
    }

    void                SetOverrideMaterial     (MaterialInstance* mi){override_material=mi;}
    MaterialInstance *  GetOverrideMaterial     ()const{return override_material;}
    void                ClearOverrideMaterial   (){override_material=nullptr;}

    MaterialInstance *  GetMaterialInstance     () const
    {
        if (override_material)
            return override_material;

        Mesh *mesh=GetMesh();

        if (!mesh)
            return nullptr;

        return mesh->GetMaterialInstance();
    }

    Material *GetMaterial() const
    {
        if (override_material)
            return override_material->GetMaterial();

        Mesh *mesh=GetMesh();

        if (!mesh)
            return nullptr;

        return mesh->GetMaterial();
    }

    const bool CanRender() const override
    {
        if (!sm_data.valid())
            return false;

        const MeshComponentData *mcd=GetData();

        if (!mcd || !mcd->mesh)
            return false;

        return true;
    }

    // 由组件创建并提交 DrawNode
    uint SubmitDrawNodes(hgl::graph::MaterialRenderMap &mrm) override
    {
        if(!CanRender()) return 0;

        // 组件创建自身的 DrawNode，然后交给 mrm 统一分派
        mrm.AddDrawNode(new hgl::graph::MeshComponentDrawNode(this));
        return 1;
    }
};//class MeshComponent

COMPONENT_NAMESPACE_END
