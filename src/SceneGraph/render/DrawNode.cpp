#include<hgl/graph/DrawNode.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/mesh/MeshNode.h>
#include<hgl/component/PrimitiveComponent.h>
#include<hgl/component/StaticMeshComponent.h>
#include<hgl/graph/SceneNode.h>

VK_NAMESPACE_BEGIN
/**
* 见排序说明
*/
const int DrawNode::compare(const DrawNode &other)const
{
    hgl::int64 off;

    auto *prim_one=other.GetPrimitive()->GetGeometry();
    auto *prim_two=this->GetPrimitive()->GetGeometry();

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

// DrawNodePrimitive
DrawNodePrimitive::DrawNodePrimitive(PrimitiveComponent *c):comp(c){}

SceneNode *DrawNodePrimitive::GetSceneNode() const
{
    return comp?comp->GetOwnerNode():nullptr;
}

Component *DrawNodePrimitive::GetComponent() const
{
    return comp;
}

Primitive *DrawNodePrimitive::GetPrimitive() const
{
    return comp?comp->GetPrimitive():nullptr;
}

MaterialInstance *DrawNodePrimitive::GetMaterialInstance() const
{
    return comp?comp->GetMaterialInstance():nullptr;
}

NodeTransform *DrawNodePrimitive::GetTransform() const
{
    return comp; // 直接使用组件自身的变换
}

// StaticMeshDrawNode（StaticMesh 用，叠加 MeshNode 变换）
DrawNodeStaticMesh::DrawNodeStaticMesh(StaticMeshComponent *c, MeshNode *meshnode, Primitive *m)
    :component(c)
    ,mesh_node(meshnode)
    ,meshnode_transform(meshnode)
    ,primitive(m){}

SceneNode *DrawNodeStaticMesh::GetSceneNode() const
{
    return component?component->GetOwnerNode():nullptr;
}

Component *DrawNodeStaticMesh::GetComponent() const
{
    return component;
}

Primitive *DrawNodeStaticMesh::GetPrimitive() const
{
    return primitive;
}

MaterialInstance *DrawNodeStaticMesh::GetMaterialInstance() const
{
    return primitive?primitive->GetMaterialInstance():nullptr;
}

void DrawNodeStaticMesh::EnsureCombined() const
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

NodeTransform *DrawNodeStaticMesh::GetTransform() const
{
    EnsureCombined();
    return const_cast<NodeTransform*>(&combined_tf);
}
VK_NAMESPACE_END

