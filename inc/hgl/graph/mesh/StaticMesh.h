#pragma once

#include <hgl/type/ObjectList.h>
#include <hgl/type/SortedSet.h>
#include <hgl/graph/mesh/SubMesh.h>
#include <hgl/graph/mesh/MeshNode.h>

VK_NAMESPACE_BEGIN

/**
* StaticMesh
* 负责管理一组 Primitive 与其对应的 SubMesh，同时集中跟踪使用到的 MaterialInstance 与 Pipeline（多实例）。
* 另外，Mesh 内部维护一个 MeshNode 树结构与一个 MeshNode 集合，并在构造时创建一个根节点。
* 注意：Mesh 拥有其内部创建的 SubMesh 的生命周期；Primitive/MaterialInstance/Pipeline 仅持有引用不管理生命周期。
*/
class StaticMesh
{
    // SubMesh / 资源集合
    ObjectList<SubMesh>           submeshes;          ///< SubMesh 列表（拥有对象）
    SortedSet<Primitive *>        primitives;         ///< 关联的 Primitive 集合（仅持引用）
    SortedSet<MaterialInstance *> mat_inst_set;       ///< 使用到的材质实例集合（仅持引用）
    SortedSet<Pipeline *>         pipeline_set;       ///< 使用到的管线集合（仅持引用）

    // MeshNode 集合与根节点
    MeshNodeList                  nodes;              ///< MeshNode 集合（拥有对象）
    MeshNode *                    root_node = nullptr;///< 根节点（由 nodes 持有）

    AABB                local_bounding_box;           ///< 所有 SubMesh 合并的本地包围盒

public:

    StaticMesh();
    virtual ~StaticMesh();

public: // MeshNode 管理
    MeshNode *                  GetRootNode     () const { return root_node; }
    const MeshNodeList &        GetNodes        () const { return nodes; }
    MeshNodeList &              GetNodes        ()       { return nodes; }

    // 将一个节点加入集合（Mesh 接管其生命周期）；若未设置父子关系，可自行将其挂到根节点下
    bool                        AddNode         (MeshNode *node) { return node ? nodes.Add(node) >= 0 : false; }

    // 从集合移除并销毁该节点（若为根节点会一并清空并重建新的空根节点）
    void                        RemoveNode      (MeshNode *node);

    // 清空所有节点并重建一个空根节点
    void                        ClearNodes      ();

public: // SubMesh 管理
    const int                   GetSubMeshCount     ()                      const{ return submeshes.GetCount(); }
    const ObjectList<SubMesh> & GetSubMeshes        ()                      const{ return submeshes; }

    // 创建并添加一个 SubMesh（为该 SubMesh 指定 Primitive / MaterialInstance / Pipeline）
    SubMesh *                   CreateSubMesh       (Primitive *prim, MaterialInstance *mi, Pipeline *p);

    // 添加一个已有的 SubMesh（Mesh 将接管其生命周期）
    bool                        AddSubMesh          (SubMesh *sm);

    // 从 StaticMesh 中移除并销毁一个 SubMesh
    void                        RemoveSubMesh       (SubMesh *sm);

    // 清空并销毁所有 SubMesh
    void                        ClearSubMeshes      ();

public: // Primitive / MaterialInstance / Pipeline（仅保存引用，便于统计/查询）
    bool                        AttachPrimitive     (Primitive *prim);
    void                        DetachPrimitive     (Primitive *prim);
    const SortedSet<Primitive *> &        GetPrimitives()         const { return primitives; }

    const SortedSet<MaterialInstance *> & GetMaterialInstances()  const { return mat_inst_set; }
    const SortedSet<Pipeline *>         & GetPipelines()          const { return pipeline_set; }

public: // 数据更新
    // 当 Primitive/VIL 数据发生变化时，更新所有 SubMesh 的渲染数据
    void                        UpdateAllSubMeshes  ();

public: // 包围盒
    void                        RefreshBoundingBox  ();
    const AABB &                GetLocalBoundingBox () const { return local_bounding_box; }

private:
    void                        RebuildResourceSets ();
};

VK_NAMESPACE_END
