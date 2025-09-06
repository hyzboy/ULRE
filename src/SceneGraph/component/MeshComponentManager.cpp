#include<hgl/component/MeshComponent.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/Mesh.h>

COMPONENT_NAMESPACE_BEGIN

//uint MeshComponentData::unique_id_count=0;

MeshComponentData::~MeshComponentData()
{
//    LogInfo(AnsiString("~MeshComponentData():")+AnsiString::numberOf(unique_id));

    if(mesh)
    {
        //mesh->Release();      //外面有MeshManager管理，不需要在这里释放.但未来要考虑是增加Release函数通知这里释放了一次使用权
        mesh=nullptr;
    }
}

Component *MeshComponentManager::CreateComponent(ComponentDataPtr cdp)
{
    if(!cdp)return(nullptr);

    if(!dynamic_cast<MeshComponentData *>(cdp.get()))
    {
        //LogError(OS_TEXT("MeshComponentManager::CreateMeshComponent: invalid component data type."));
        return(nullptr);
    }

    return(new MeshComponent(cdp,this));
}

Component *MeshComponentManager::CreateComponent(Mesh *m)
{
    ComponentDataPtr cdp=new MeshComponentData(m);

    return CreateComponent(cdp);
}

COMPONENT_NAMESPACE_END
