#pragma once

#include<hgl/component/RenderComponent.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/RenderBatchMap.h>

COMPONENT_NAMESPACE_BEGIN

class PrimitiveComponent;
class PrimitiveComponentManager;

struct PrimitiveComponentData:public ComponentData
{
    //static uint unique_id_count;

    //uint unique_id;

    Primitive *primitive;

public:

    PrimitiveComponentData()
    {
        primitive=nullptr;

//        unique_id=++unique_id_count;
//        LogInfo(AnsiString("PrimitiveComponentData():")+AnsiString::numberOf(unique_id));
    }

    PrimitiveComponentData(Primitive *m)
    {
        primitive=m;

//        unique_id=++unique_id_count;
//        LogInfo(AnsiString("PrimitiveComponentData(Primitive *):")+AnsiString::numberOf(unique_id));
    }

    virtual ~PrimitiveComponentData();

    COMPONENT_DATA_CLASS_BODY(Primitive)
};//struct PrimitiveComponentData

class PrimitiveComponentManager:public ComponentManager
{
public:

    COMPONENT_MANAGER_CLASS_BODY(Primitive)

public:

    PrimitiveComponentManager()=default;

    Component *CreateComponent(ComponentDataPtr cdp) override;

    Component *CreateComponent(Primitive *);
};//class PrimitiveComponentManager

class PrimitiveComponent:public RenderComponent
{
    WeakPtr<ComponentData> primitive_data;
    MaterialInstance* override_material = nullptr; // 新增

public:

    COMPONENT_CLASS_BODY(Primitive)

public:

    PrimitiveComponent(ComponentDataPtr cdp,PrimitiveComponentManager *cm):RenderComponent(cdp,cm)
    {
        primitive_data=cdp;
    }

    virtual ~PrimitiveComponent()=default;

    virtual Component *Clone() override
    {
        PrimitiveComponent *mc=(PrimitiveComponent *)RenderComponent::Clone();

        if(!mc)
            return(mc);

        mc->override_material=override_material;
        return mc;
    }

            PrimitiveComponentData *GetData()      {return dynamic_cast<      PrimitiveComponentData *>(primitive_data.get());}
    const   PrimitiveComponentData *GetData()const {return dynamic_cast<const PrimitiveComponentData *>(primitive_data.const_get());}

    Primitive *GetPrimitive()const
    {
        if(!primitive_data.valid())
            return(nullptr);

        const PrimitiveComponentData *mcd=dynamic_cast<const PrimitiveComponentData *>(primitive_data.const_get());

        if(!mcd)
            return(nullptr);

        return mcd->primitive;
    }

    const bool GetLocalAABB(AABB &box) const override
    {
        Primitive *primitive=GetPrimitive();

        if (!primitive)
            return false;

        box=primitive->GetBoundingVolumes().aabb;
        return true;
    }

public:

    Pipeline *GetPipeline() const
    {
        Primitive *primitive=GetPrimitive();

        if (!primitive)
            return nullptr;

        return primitive->GetPipeline();
    }

    void                SetOverrideMaterial     (MaterialInstance* mi){override_material=mi;}
    MaterialInstance *  GetOverrideMaterial     ()const{return override_material;}
    void                ClearOverrideMaterial   (){override_material=nullptr;}

    MaterialInstance *  GetMaterialInstance     () const
    {
        if (override_material)
            return override_material;

        Primitive *primitive=GetPrimitive();

        if (!primitive)
            return nullptr;

        return primitive->GetMaterialInstance();
    }

    Material *GetMaterial() const
    {
        if (override_material)
            return override_material->GetMaterial();

        Primitive *primitive=GetPrimitive();

        if (!primitive)
            return nullptr;

        return primitive->GetMaterial();
    }

    const bool CanRender() const override
    {
        if (!primitive_data.valid())
            return false;

        const PrimitiveComponentData *mcd=GetData();

        if (!mcd || !mcd->primitive)
            return false;

        return true;
    }

    // 由组件创建并提交 DrawNode
    uint SubmitDrawNodes(hgl::graph::RenderBatchMap &mrm) override
    {
        if(!CanRender()) return 0;

        const PrimitiveComponentData *mcd=GetData();

        if (!mcd || !mcd->primitive)
            return 0;

        // 组件创建自身的 DrawNode，然后交给 mrm 统一分派
        mrm.AddDrawNode(new hgl::graph::DrawNodePrimitive(this));
        return 1;
    }
};//class PrimitiveComponent

COMPONENT_NAMESPACE_END
