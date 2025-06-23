#pragma once

#include<hgl/component/RenderComponent.h>
#include<hgl/graph/Mesh.h>

//#include<hgl/log/LogInfo.h>

COMPONENT_NAMESPACE_BEGIN

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
//        LOG_INFO(AnsiString("MeshComponentData():")+AnsiString::numberOf(unique_id));
    }

    MeshComponentData(Mesh *m)
    {
        mesh=m;

//        unique_id=++unique_id_count;
//        LOG_INFO(AnsiString("MeshComponentData(Mesh *):")+AnsiString::numberOf(unique_id));
    }

    virtual ~MeshComponentData();

    COMPONENT_DATA_CLASS_BODY(Mesh)
};//struct MeshComponentData

class MeshComponent;

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

public:

    COMPONENT_CLASS_BODY(Mesh)

public:

    MeshComponent(ComponentDataPtr cdp,MeshComponentManager *cm):RenderComponent(cdp,cm)
    {
        sm_data=cdp;
    }

    virtual ~MeshComponent()=default;

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
};//class MeshComponent

COMPONENT_NAMESPACE_END
