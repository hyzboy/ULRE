#pragma once

#include<hgl/type/ObjectList.h>
#include<hgl/type/SortedSet.h>
#include<hgl/type/IDName.h>
#include<hgl/graph/NodeTransform.h>
#include<hgl/graph/AABB.h>
#include<hgl/graph/OBB.h>
#include<hgl/graph/mesh/SubMesh.h>

namespace hgl::graph
{
    using MeshNodeID = int64;

    class MeshNode;
    using MeshNodeList = ObjectList<MeshNode>;
    using MeshNodeSet = SortedSet<MeshNode *>;
    using SubMeshSet = SortedSet<SubMesh *>;

    HGL_DEFINE_U16_IDNAME(MeshNodeName)

    /**
    * Mesh 节点
    * 与 SceneNode 结构类似，但不绑定 Scene、不包含 Component，仅用于模型内部的节点层级与 SubMesh 引用管理。
    * 注意：所有 MeshNode 的生命周期由 Mesh 统一管理。因此子节点集合为非拥有型容器，避免重复释放。
    */
    class MeshNode:public NodeTransform
    {
        MeshNode *      parent_node=nullptr;          ///< 父节点

        MeshNodeID      node_id=-1;                   ///< 节点ID
        MeshNodeName    node_name;                    ///< 节点名称

    protected:

        AABB local_bounding_box;                      ///< 本地坐标包围盒
        OBB  world_bounding_box;                      ///< 世界坐标包围盒

        MeshNodeSet child_nodes;                            ///< 子节点（非拥有型，避免重复释放）
        SubMeshSet  submesh_set;                            ///< SubMesh 集合（仅持有引用，不管理生命周期）

    public:

        MeshNode():NodeTransform(){}
        MeshNode(const NodeTransform &so):NodeTransform(so){}
        MeshNode(const Matrix4f &mat):NodeTransform(mat){}
        virtual ~MeshNode();

    public:

        // 基本属性
        const   MeshNodeID &    GetNodeID   ()const { return node_id; }
        const   MeshNodeName &  GetNodeName ()const { return node_name; }

        const   MeshNodeSet &   GetChildNode()const { return child_nodes; }

        // 关系
                void            SetParent   (MeshNode *pn)  { parent_node=pn; }
                MeshNode *      GetParent   ()      noexcept{ return parent_node; }
        const   MeshNode *      GetParent   ()const noexcept{ return parent_node; }

        MeshNode *AddChild(MeshNode *sn)
        {
            if(!sn)return nullptr;
            child_nodes.Add(sn);
            sn->SetParent(this);
            return sn;
        }

    public:

        // 克隆/重置
        virtual MeshNode *  CreateNode()const { return new MeshNode(); }
        virtual void        CloneChildren(MeshNode *node) const
        {
            for(MeshNode *sn:GetChildNode())
                node->AddChild(sn->Clone());
        }
        virtual void        CloneSubMeshes(MeshNode *node) const
        {
            for(SubMesh *sm:submesh_set)
                node->AttachSubMesh(sm);
        }
        virtual MeshNode *  Clone() const;
        virtual void        Reset() override;

    public: // 坐标/包围盒

        virtual void        UpdateWorldTransform() override;            ///< 刷新世界变换
        virtual void        RefreshBoundingBox();                       ///< 刷新包围盒，合并自身 SubMesh 与子节点

        virtual const AABB &GetLocalBoundingBox()const { return local_bounding_box; }

    public: // SubMesh 相关

                      bool          SubMeshIsEmpty      ()const{return submesh_set.IsEmpty();}
        virtual const int64         GetSubMeshCount     ()const{return submesh_set.GetCount();}
        virtual       bool          AttachSubMesh       (SubMesh *sm)
        {
            if(!sm)return(false);
            return submesh_set.Add(sm)>=0;
        }
        virtual         void        DetachSubMesh       (SubMesh *sm)
        {
            if (!sm)return; submesh_set.Delete(sm);
        }
                      bool          Contains            (SubMesh *sm){return submesh_set.Contains(sm);}     ///< 是否包含指定 SubMesh
                const SubMeshSet &  GetSubMeshes        ()const{return submesh_set;}
    };//class MeshNode
}//namespace hgl::graph
