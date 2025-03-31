#include<hgl/component/StaticMeshComponent.h>
#include<hgl/graph/SceneNode.h>

COMPONENT_NAMESPACE_BEGIN

size_t StaticMeshComponentManager::ComponentHashCode()const
{
    return GetTypeHash<StaticMeshComponentManager>();
}

StaticMeshComponent *StaticMeshComponentManager::CreateStaticMeshComponent(SceneNode *psn,StaticMeshComponentData *data)
{
    if(!psn||!data)return(nullptr);

    StaticMeshComponent *smc=new StaticMeshComponent(psn,data,this);

    psn->AddComponent(smc);

    return smc;
}

Component *StaticMeshComponentManager::CreateComponent(SceneNode *psn,ComponentData *data)
{
    return CreateStaticMeshComponent(psn,reinterpret_cast<StaticMeshComponentData *>(data));
}

COMPONENT_NAMESPACE_END
