#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/VKRenderResource.h>

VK_NAMESPACE_BEGIN

//StaticMesh *StaticMesh::CreateNewObject(RenderResource *rr,SceneNode *node)
//{
//    if(!node)
//        return(nullptr);
//
//    if(node->IsEmpty())
//        return(nullptr);
//
//    return(new StaticMesh(rr,node));
//}

StaticMesh::StaticMesh(SceneNode *sn)
{
    root_node=sn;
}

StaticMesh::~StaticMesh()
{
    SAFE_CLEAR(root_node);
}

VK_NAMESPACE_END
