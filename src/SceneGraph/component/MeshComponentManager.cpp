#include<hgl/component/MeshComponent.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/Mesh.h>

COMPONENT_NAMESPACE_BEGIN

MeshComponentData::~MeshComponentData()
{
    if(mesh)
    {
        //mesh->Release();      //外面有RenderResource管理，不需要在这里释放.但未来要考虑是增加Release函数通知这里释放了一次使用权
        mesh=nullptr;
    }
}

Component *MeshComponentManager::CreateComponent(ComponentData *data)
{
    if(!data)return(nullptr);

    return CreateComponent(reinterpret_cast<MeshComponentData *>(data));
}

MeshComponent *MeshComponentManager::CreateComponent(MeshComponentData *data)
{
    if(!data)return(nullptr);

    return(new MeshComponent(data,this));
}

COMPONENT_NAMESPACE_END
