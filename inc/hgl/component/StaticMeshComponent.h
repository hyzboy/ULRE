#pragma once

#include<hgl/component/PrimitiveComponent.h>
#include<hgl/graph/Mesh.h>

COMPONENT_NAMESPACE_BEGIN

struct StaticMeshComponentData:public ComponentData
{
    Mesh *renderable;
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

    StaticMeshComponent *CreateStaticMeshComponent(SceneNode *psn,StaticMeshComponentData *data);

    virtual Component *CreateComponent(SceneNode *psn,ComponentData *data) override;
};//class StaticMeshComponentManager

class StaticMeshComponent:public PrimitiveComponent
{
    StaticMeshComponentData *sm_data;

public:

    StaticMeshComponent(SceneNode *psn,ComponentData *cd,ComponentManager *cm)
        :PrimitiveComponent(psn,cd,cm)
    {
        sm_data=reinterpret_cast<StaticMeshComponentData *>(cd);
    }

    virtual ~StaticMeshComponent()=default;

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

};//class StaticMeshComponent

COMPONENT_NAMESPACE_END
