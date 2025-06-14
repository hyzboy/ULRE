#pragma once

#include<hgl/component/RenderComponent.h>
#include<hgl/graph/Mesh.h>

COMPONENT_NAMESPACE_BEGIN

struct StaticMeshComponentData:public ComponentData
{
    Mesh *mesh;

public:

    StaticMeshComponentData()
    {
        mesh=nullptr;
    }

    StaticMeshComponentData(Mesh *m)
    {
        mesh=m;
    }

    virtual ~StaticMeshComponentData();
};//struct StaticMeshComponentData

class StaticMeshComponent;

class StaticMeshComponentManager:public ComponentManager
{
public:

    static StaticMeshComponentManager *GetDefaultManager()
    {
        return GetComponentManager<StaticMeshComponentManager>(true);
    }

    static constexpr const size_t StaticHashCode          (){return hgl::GetTypeHash<StaticMeshComponentManager>();}
    static constexpr const size_t StaticComponentHashCode (){return hgl::GetTypeHash<StaticMeshComponent>();}

    const size_t GetComponentHashCode   ()const override{return StaticMeshComponentManager::StaticComponentHashCode();}
    const size_t GetHashCode            ()const override{return StaticMeshComponentManager::StaticHashCode();}

public:

    StaticMeshComponentManager()=default;

    StaticMeshComponent *CreateComponent(StaticMeshComponentData *data);

    StaticMeshComponent *CreateComponent(Mesh *m)
    {
        auto sm_cd=new StaticMeshComponentData(m);

        return CreateComponent(sm_cd);
    }

    virtual Component *CreateComponent(ComponentData *data) override;
};//class StaticMeshComponentManager

class StaticMeshComponent:public RenderComponent
{
    StaticMeshComponentData *sm_data;

public:

    StaticMeshComponent(StaticMeshComponentData *cd,StaticMeshComponentManager *cm):RenderComponent(cd,cm){sm_data=cd;}

    virtual ~StaticMeshComponent()=default;

    static StaticMeshComponentManager *GetDefaultManager()
    {
        return StaticMeshComponentManager::GetDefaultManager();
    }

    static constexpr const size_t StaticHashCode()
    {
        return hgl::GetTypeHash<StaticMeshComponent>();
    }

    const size_t    GetHashCode()const override
    {
        return StaticMeshComponent::StaticHashCode();
    }

            StaticMeshComponentData &GetData()      {return *sm_data;}
    const   StaticMeshComponentData &GetData()const {return *sm_data;}

    Mesh *GetMesh() const{return sm_data->mesh;}
};//class StaticMeshComponent

COMPONENT_NAMESPACE_END
