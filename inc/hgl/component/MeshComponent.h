#pragma once

#include<hgl/component/RenderComponent.h>
#include<hgl/graph/Mesh.h>

COMPONENT_NAMESPACE_BEGIN

struct MeshComponentData:public ComponentData
{
    Mesh *mesh;

public:

    MeshComponentData()
    {
        mesh=nullptr;
    }

    MeshComponentData(Mesh *m)
    {
        mesh=m;
    }

    virtual ~MeshComponentData();

    ComponentData *Duplication() override
    {
        return(new MeshComponentData(mesh));
    }
};//struct MeshComponentData

class MeshComponent;

class MeshComponentManager:public ComponentManager
{
public:

    static MeshComponentManager *GetDefaultManager()
    {
        return GetComponentManager<MeshComponentManager>(true);
    }

    static constexpr const size_t StaticHashCode          (){return hgl::GetTypeHash<MeshComponentManager>();}
    static constexpr const size_t StaticComponentHashCode (){return hgl::GetTypeHash<MeshComponent>();}

    const size_t GetComponentHashCode   ()const override{return MeshComponentManager::StaticComponentHashCode();}
    const size_t GetHashCode            ()const override{return MeshComponentManager::StaticHashCode();}

public:

    MeshComponentManager()=default;

    MeshComponent *CreateComponent(MeshComponentData *data);

    MeshComponent *CreateComponent(Mesh *m)
    {
        auto sm_cd=new MeshComponentData(m);

        return CreateComponent(sm_cd);
    }

    virtual Component *CreateComponent(ComponentData *data) override;
};//class MeshComponentManager

class MeshComponent:public RenderComponent
{
    MeshComponentData *sm_data;

public:

    COMPONENT_CLASS_BODY(Mesh)

public:

    MeshComponent(MeshComponentData *cd,MeshComponentManager *cm):RenderComponent(cd,cm){sm_data=cd;}

    virtual ~MeshComponent()=default;

            MeshComponentData &GetData()      {return *sm_data;}
    const   MeshComponentData &GetData()const {return *sm_data;}

    Mesh *GetMesh() const{return sm_data->mesh;}
};//class MeshComponent

COMPONENT_NAMESPACE_END
