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

    void MeshNode::RefreshBoundingBox()
    {
        AABB local;
        bool has_box=false;

        // 合并自身 submesh 盒子
        for(Mesh *sm:submesh_set)
        {
            if(!sm)continue;

            if(!has_box)
            {
                local=sm->GetBoundingBox();
                has_box=true;
            }
            else
            {
                local.Enclose(sm->GetBoundingBox());
            }
        }

        // 合并子节点盒子
        for(MeshNode *sn:child_nodes)
        {
            if(!sn)continue;
            sn->RefreshBoundingBox();

            if(!has_box)
            {
                local=sn->GetLocalBoundingBox();
                has_box=true;
            }
            else
            {
                local.Enclose(sn->GetLocalBoundingBox());
            }
        }

        if(has_box)
            local_bounding_box=local;
        else
            local_bounding_box.Clear();
    }

    MeshNode::~MeshNode()
    {
        SetParent(nullptr);
        submesh_set.Clear();
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

        local_bounding_box.Clear();
        world_bounding_box.Clear();

        child_nodes.Clear();
        submesh_set.Clear();

        NodeTransform::Reset();
    }
}//namespace hgl::graph
