#pragma once

#include <hgl/type/ObjectList.h>
#include <hgl/type/SortedSet.h>
#include <hgl/type/ArrayList.h>
#include <hgl/graph/mesh/Mesh.h>
#include <hgl/graph/mesh/MeshNode.h>

VK_NAMESPACE_BEGIN

using GeometryPtrSet        =SortedSet<Geometry *>;
using MaterialInstanceSet   =SortedSet<MaterialInstance *>;
using PipelinePtrSet        =SortedSet<Pipeline *>;

/**
* StaticMesh
* 负责管理一组 Geometry 与其对应的 Mesh，同时集中跟踪使用到的 MaterialInstance 与 Pipeline（多实例）。
* 另外，Mesh 内部维护一个 MeshNode 树结构与一个 MeshNode 集合，并在构造时创建一个根节点。
* 注意：Mesh 拥有其内部创建的 Mesh 的生命周期；Geometry/MaterialInstance/Pipeline 仅持有引用不管理生命周期。
*/
class StaticMesh
{
    // Mesh / 资源集合
    ObjectList<Mesh>    submeshes;                                                                                  ///< Mesh 列表(拥有对象)
    GeometryPtrSet      geometry_set;                                                                               ///< 关联的 Geometry 集合(仅持引用)
    MaterialInstanceSet mat_inst_set;                                                                               ///< 使用到的材质实例集合(仅持引用)
    PipelinePtrSet      pipeline_set;                                                                               ///< 使用到的管线集合(仅持引用)

    // MeshNode 集合与根节点
    MeshNodeList        nodes;                                                                                      ///< MeshNode 集合(拥有对象)
    MeshNode *          root_node = nullptr;                                                                        ///< 根节点(由 nodes 持有)

    AABB                bounding_box;                                                                               ///< 所有 Mesh 合并的本地包围盒

public:

    StaticMesh();
    virtual ~StaticMesh();

public: // MeshNode 管理

    MeshNode *                  GetRootNode         () const { return root_node; }
    const MeshNodeList &        GetNodes            () const { return nodes; }
    MeshNodeList &              GetNodes            ()       { return nodes; }

    bool                        AddNode             (MeshNode *node) { return node ? nodes.Add(node) >= 0 : false; }   ///< 将一个节点加入集合(Mesh 接管其生命周期);若未设置父子关系,可自行将其挂到根节点下

    void                        RemoveNode          (MeshNode *node);                                               ///< 从集合移除并销毁该节点(若为根节点会一并清空并重建新的空根节点)

    void                        ClearNodes          ();                                                             ///< 清空所有节点并重建一个空根节点

public: // Mesh 管理

    const int                   GetSubMeshCount     ()                      const{ return submeshes.GetCount(); }
    const ObjectList<Mesh> &    GetSubMeshes        ()                      const{ return submeshes; }

    Mesh *                      CreateMesh          (Geometry *prim, MaterialInstance *mi, Pipeline *p);            ///< 创建并添加一个 Mesh(为该 Mesh 指定 Geometry / MaterialInstance / Pipeline)

    bool                        AddSubMesh          (Mesh *sm);                                                     ///< 添加一个已有的 Mesh(Mesh 将接管其生命周期)

    void                        RemoveSubMesh       (Mesh *sm);                                                     ///< 从 StaticMesh 中移除并销毁一个 Mesh

    void                        ClearSubMeshes      ();                                                             ///< 清空并销毁所有 Mesh

public: // Geometry / MaterialInstance / Pipeline(仅保存引用,便于统计/查询)

    bool                        AttachGeometry      (Geometry *prim);
    void                        DetachGeometry      (Geometry *prim);
    const GeometryPtrSet &      GetGeometries       () const { return geometry_set; }

    const MaterialInstanceSet & GetMaterialInstances() const { return mat_inst_set; }
    const PipelinePtrSet &      GetPipelines        () const { return pipeline_set; }

public: // 数据更新

    void                        UpdateAllSubMeshes  ();                                                             ///< 当 Geometry/VIL 数据发生变化时,更新所有 Mesh 的渲染数据

public: // 包围盒

    void                        RefreshBoundingBox  ();
    const AABB &                GetBoundingBox      () const { return bounding_box; }

private:

    void                        RebuildResourceSets ();
};

VK_NAMESPACE_END
