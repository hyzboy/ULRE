#include<hgl/graph/mesh/MeshNode.h>

namespace hgl::graph
{
    void MeshNode::UpdateWorldTransform()
    {
        NodeTransform::UpdateWorldTransform();

        const Matrix4f &l2w=transform_state.GetLocalToWorldMatrix();

        for(MeshNode *sub:child_nodes)
        {
            sub->SetParentMatrix(l2w);
            sub->UpdateWorldTransform();
        }
    }

    void MeshNode::RefreshBoundingVolumes()
    {
        AABB local;
        bool has_box=false;

        // 合并自身 submesh 盒子
        for(Primitive *sm:primitive_set)
        {
            if(!sm)continue;

            if(!has_box)
            {
                local=sm->GetBoundingVolumes().aabb;
                has_box=true;
            }
            else
            {
                local.Merge(sm->GetBoundingVolumes().aabb);
            }
        }

        // 合并子节点盒子
        for(MeshNode *sn:child_nodes)
        {
            if(!sn)continue;
            sn->RefreshBoundingVolumes();

            if(!has_box)
            {
                local=sn->GetLocalBoundingVolumes().aabb;
                has_box=true;
            }
            else
            {
                local.Merge(sn->GetLocalBoundingVolumes().aabb);
            }
        }

        if(has_box)
            local_bounding_volumes.SetFromAABB(local);
        else
            local_bounding_volumes.Clear();
    }

    MeshNode::~MeshNode()
    {
        SetParent(nullptr);
        primitive_set.Clear();
    }

    MeshNode *MeshNode::Clone() const
    {
        MeshNode *node=new MeshNode();
        if(!node)return nullptr;

        node->SetTransformState(GetTransformState());

        CloneChildren(node);
        CloneSubMeshes(node);

        return node;
    }

    void MeshNode::Reset()
    {
        SetParent(nullptr);

        local_bounding_volumes.Clear();

        child_nodes.Clear();
        primitive_set.Clear();

        NodeTransform::Reset();
    }
}//namespace hgl::graph
