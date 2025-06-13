#include<hgl/component/StaticMeshComponent.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/Mesh.h>

COMPONENT_NAMESPACE_BEGIN

StaticMeshComponentData::~StaticMeshComponentData()
{
    SAFE_CLEAR(mesh);
}

Component *StaticMeshComponentManager::CreateComponent(ComponentData *data)
{
    if(!data)return(nullptr);

    return CreateStaticMeshComponent(reinterpret_cast<StaticMeshComponentData *>(data));
}

StaticMeshComponent *StaticMeshComponentManager::CreateStaticMeshComponent(StaticMeshComponentData *data)
{
    if(!data)return(nullptr);

    return(new StaticMeshComponent(data,this));
}

COMPONENT_NAMESPACE_END
