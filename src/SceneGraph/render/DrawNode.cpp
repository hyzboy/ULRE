#include<hgl/graph/DrawNode.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/mesh/Mesh.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/graph/SceneNode.h>

VK_NAMESPACE_BEGIN
/**
* 见排序说明
*/
const int DrawNode::compare(const DrawNode &other)const
{
    hgl::int64 off;

    auto *prim_one=other.GetMesh()->GetPrimitive();
    auto *prim_two=this->GetMesh()->GetPrimitive();

    //比较VDM
    if(prim_one->GetVDM())      //有VDM
    {
        off=prim_one->GetVDM()
           -prim_two->GetVDM();

        if(off)
            return off;

        //比较模型
        {
            off=(hgl::int64)prim_one
               -(hgl::int64)prim_two;

            if(off)
            {
                off=prim_one->GetVertexOffset()-prim_two->GetVertexOffset();        //保证vertex offset小的在前面

                return off;
            }
        }
    }

    //比较距离（备注：正负方向等实际测试后再调整）
    float foff=other.to_camera_distance-to_camera_distance;

    if(foff>0)
        return 1;
    else
        return -1;
}

// MeshComponentDrawNode
MeshComponentDrawNode::MeshComponentDrawNode(MeshComponent *c):comp(c){}

SceneNode *MeshComponentDrawNode::GetSceneNode() const
{
    return comp?comp->GetOwnerNode():nullptr;
}

Component *MeshComponentDrawNode::GetComponent() const
{
    return comp;
}

Mesh *MeshComponentDrawNode::GetMesh() const
{
    return comp?comp->GetMesh():nullptr;
}

MaterialInstance *MeshComponentDrawNode::GetMaterialInstance() const
{
    return comp?comp->GetMaterialInstance():nullptr;
}

NodeTransform *MeshComponentDrawNode::GetTransform() const
{
    return comp; // 直接使用组件自身的变换
}

// OwnerMeshDrawNode
OwnerMeshDrawNode::OwnerMeshDrawNode(SceneNode *sn, Component *c, NodeTransform *t, Mesh *m)
    :scene_node(sn),component(c),transform(t),mesh(m){}

SceneNode *OwnerMeshDrawNode::GetSceneNode() const
{
    return scene_node;
}

Component *OwnerMeshDrawNode::GetComponent() const
{
    return component;
}

Mesh *OwnerMeshDrawNode::GetMesh() const
{
    return mesh;
}

MaterialInstance *OwnerMeshDrawNode::GetMaterialInstance() const
{
    return mesh?mesh->GetMaterialInstance():nullptr;
}

NodeTransform *OwnerMeshDrawNode::GetTransform() const
{
    return transform;
}
VK_NAMESPACE_END

