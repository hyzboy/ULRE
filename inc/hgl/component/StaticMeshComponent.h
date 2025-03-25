#pragma once

#include<hgl/component/PrimitiveComponent.h>
#include<hgl/graph/VKRenderable.h>

COMPONENT_NAMESPACE_BEGIN

struct StaticMeshComponentData:public ComponentData
{
    Renderable *renderable;
};//struct StaticMeshComponentData

class StaticMeshComponent:public PrimitiveComponent;

class StaticMeshComponentManager:public ComponentManager
{
public:

    StaticMeshComponentManager()=default;

    StaticMeshComponent *CreateStaticMeshComponent(SceneNode *psn,StaticMeshComponentData *data);

    virtual Component *CreateComponent(SceneNode *psn,ComponentData *data) override
    {
        return CreateStaticMeshComponent(psn,reinterpret_cast<StaticMeshComponentData *>(data));
    }
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

    virtual ~StaticMeshComponent()
    {
        SAFE_CLEAR(sm_Data);
    }

            StaticMeshComponentData &GetData()      {return *sm_data;}
    const   StaticMeshComponentData &GetData()const {return *sm_data;}

};//class StaticMeshComponent

COMPONENT_NAMESPACE_END
