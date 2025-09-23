#include<hgl/graph/DrawNode.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/mesh/Mesh.h>
#include<hgl/graph/mesh/MeshNode.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/component/StaticMeshComponent.h>
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

// MeshDrawNode
MeshDrawNode::MeshDrawNode(MeshComponent *c):comp(c){}

SceneNode *MeshDrawNode::GetSceneNode() const
{
    return comp?comp->GetOwnerNode():nullptr;
}

Component *MeshDrawNode::GetComponent() const
{
    return comp;
}

Mesh *MeshDrawNode::GetMesh() const
{
    return comp?comp->GetMesh():nullptr;
}

MaterialInstance *MeshDrawNode::GetMaterialInstance() const
{
    return comp?comp->GetMaterialInstance():nullptr;
}

NodeTransform *MeshDrawNode::GetTransform() const
{
    return comp; // 直接使用组件自身的变换
}

// StaticMeshDrawNode（StaticMesh 用，叠加 MeshNode 变换）
StaticMeshDrawNode::StaticMeshDrawNode(StaticMeshComponent *c, MeshNode *meshnode, Mesh *m)
    :component(c)
    ,mesh_node(meshnode)
    ,meshnode_transform(meshnode)
    ,mesh(m){}

SceneNode *StaticMeshDrawNode::GetSceneNode() const
{
    return component?component->GetOwnerNode():nullptr;
}

Component *StaticMeshDrawNode::GetComponent() const
{
    return component;
}

Mesh *StaticMeshDrawNode::GetMesh() const
{
    return mesh;
}

MaterialInstance *StaticMeshDrawNode::GetMaterialInstance() const
{
    return mesh?mesh->GetMaterialInstance():nullptr;
}

void StaticMeshDrawNode::EnsureCombined() const
{
    NodeTransform *scenenode_transform = component ? static_cast<NodeTransform *>(const_cast<StaticMeshComponent *>(component)) : nullptr;

    const uint32 ov = scenenode_transform ? scenenode_transform->GetTransformVersion() : 0;
    const uint32 mv = meshnode_transform ? meshnode_transform->GetTransformVersion() : 0;

    if(ov!=scenenode_ver_cache || mv!=meshnode_ver_cache)
    {
        const Matrix4f owner_l2w    = scenenode_transform ? scenenode_transform->GetLocalToWorldMatrix()    : Identity4f;
        const Matrix4f meshnode_l2w = meshnode_transform  ? meshnode_transform->GetLocalToWorldMatrix() : Identity4f;

        const Matrix4f combined = owner_l2w * meshnode_l2w;

        combined_tf.SetParentMatrix(Identity4f);
        combined_tf.SetLocalMatrix(combined);
        combined_tf.UpdateWorldTransform();

        scenenode_ver_cache = ov;
        meshnode_ver_cache = mv;
    }
}

NodeTransform *StaticMeshDrawNode::GetTransform() const
{
    EnsureCombined();
    return const_cast<NodeTransform*>(&combined_tf);
}
VK_NAMESPACE_END

