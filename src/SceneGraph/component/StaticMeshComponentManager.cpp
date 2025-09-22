#include<hgl/component/StaticMeshComponent.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/mesh/StaticMesh.h>

COMPONENT_NAMESPACE_BEGIN

Component *StaticMeshComponentManager::CreateComponent(ComponentDataPtr cdp)
{
    if(!cdp)return nullptr;

    if(!dynamic_cast<StaticMeshComponentData *>(cdp.get()))
        return nullptr;

    return new StaticMeshComponent(cdp,this);
}

Component *StaticMeshComponentManager::CreateComponent(hgl::graph::StaticMesh *m)
{
    ComponentDataPtr cdp=new StaticMeshComponentData(m);
    return CreateComponent(cdp);
}

COMPONENT_NAMESPACE_END
