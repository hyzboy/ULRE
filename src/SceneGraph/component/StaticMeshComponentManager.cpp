#include<hgl/component/StaticMeshComponent.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/Mesh.h>

COMPONENT_NAMESPACE_BEGIN

StaticMeshComponentData::~StaticMeshComponentData()
{
    if(mesh)
    {
        //mesh->Release();      //外面有RenderResource管理，不需要在这里释放.但未来要考虑是增加Release函数通知这里释放了一次使用权
        mesh=nullptr;
    }
}

Component *StaticMeshComponentManager::CreateComponent(ComponentData *data)
{
    if(!data)return(nullptr);

    return CreateComponent(reinterpret_cast<StaticMeshComponentData *>(data));
}

StaticMeshComponent *StaticMeshComponentManager::CreateComponent(StaticMeshComponentData *data)
{
    if(!data)return(nullptr);

    return(new StaticMeshComponent(data,this));
}

COMPONENT_NAMESPACE_END
