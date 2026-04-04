#pragma once

#include <hgl/type/ManagedArray.h>
#include <hgl/type/OrderedSet.h>
#include <hgl/type/ValueArray.h>
#include <hgl/graph/mesh/Primitive.h>
#include <string>
#include <vector>
#include <cstdint>

namespace hgl::graph{

/**
 * StaticMeshNode - 场景树节点
 * 描述一个场景节点的变换、包围体和引用到primitive_list的图元索引
 */
struct StaticMeshNode
{
    std::string          name;
    int32_t              parentIndex     = -1;              ///< -1 表示根节点
    std::vector<int32_t> children;                          ///< 子节点在 nodes_ 中的索引
    Matrix4f             localMatrix     { 1.0f };          ///< 本地变换矩阵
    Matrix4f             worldMatrix     { 1.0f };          ///< 世界变换矩阵
    bool                 hasTRS          = false;
    Vector3f             translation     {};
    Quatf                rotation        { 1.0f, 0.0f, 0.0f, 0.0f };  ///< w,x,y,z
    Vector3f             scale           { 1.0f, 1.0f, 1.0f };
    BoundingVolumes      nodeBounds;
    bool                 boundsValid     = false;
    std::vector<int32_t> primitiveIndices;                  ///< 引用 StaticMesh::primitive_list 的下标
};//struct StaticMeshNode

using GeometryPtrSet        =OrderedSet<Geometry *>;
using MaterialInstanceSet   =OrderedSet<MaterialInstance *>;
using PipelinePtrSet        =OrderedSet<GraphicsPipeline *>;
using PrimitiveList         =ManagedArray<Primitive>;

/**
* StaticMesh
* 多个Primitive的集合体
*/
class StaticMesh
{
    // Primitive / 资源集合
    GeometryPtrSet          geometry_set;                                                                               ///< 关联的 Geometry 集合(仅持引用)
    MaterialInstanceSet     mat_inst_set;                                                                               ///< 使用到的材质实例集合(仅持引用)
    PipelinePtrSet          pipeline_set;                                                                               ///< 使用到的管线集合(仅持引用)

    PrimitiveList           primitive_list;                                                                             ///< Primitive列表

    BoundingVolumes   bounding_volumes;                                                                           ///< 所有 Primitive 合并的本地包围体

    // Pipeline 缓存映射（Primitive* -> GraphicsPipeline*）
    std::unordered_map<Primitive *, GraphicsPipelinePreRaster *> _primitive_pipeline_cache;

    // 场景树
    std::vector<StaticMeshNode> nodes_;
    std::vector<int32_t>        rootNodes_;                                                                       ///< nodes_ 中根节点的下标

public:

    StaticMesh();
    virtual ~StaticMesh();

public: // Geometry / MaterialInstanceData / GraphicsPipeline(仅保存引用,便于统计/查询)

    bool                        AttachGeometry      (Geometry *geometry);
    void                        DetachGeometry      (Geometry *geometry);
    const GeometryPtrSet &      GetGeometries       () const { return geometry_set; }

    const MaterialInstanceSet & GetMaterialInstances() const { return mat_inst_set; }
    const PipelinePtrSet &      GetPipelines        () const { return pipeline_set; }

public: // Primitive 管理

    const int                   GetPrimitiveCount   ()const{ return primitive_list.GetCount(); }
    const PrimitiveList &       GetPrimitiveList    ()const{ return primitive_list; }

    Primitive *                 CreatePrimitive     (Geometry *geometry, MaterialInstance *mi, GraphicsPipeline *p);            ///< 创建并添加一个 Primitive(为该 Primitive 指定 Geometry / MaterialInstance / GraphicsPipeline)

    bool                        AddPrimitive        (Primitive *sm);                                                    ///< 添加一个已有的 Primitive(StaticMesh 将接管其生命周期)

    void                        RemovePrimitive     (Primitive *sm);                                                    ///< 从 StaticMesh 中移除并销毁一个 Primitive

    void                        ClearPrimitives     ();                                                                 ///< 清空并销毁所有 Primitive

    void                        UpdatePrimitives  ();                                                                   ///< 当 Geometry/VIL 数据发生变化时,更新所有 Primitive 的渲染数据

public: // 包围盒

    void                        RefreshBoundingVolumes  ();
    const BoundingVolumes &     GetBoundingVolumes      () const { return bounding_volumes; }

private:

    void                        RebuildResourceSets ();

public: // 场景树

    int32_t                                 AddNode         (StaticMeshNode &&node);            ///< 追加节点，返回其在 nodes_ 里的下标
    void                                    SetRootNodes    (std::vector<int32_t> roots){ rootNodes_ = std::move(roots); }

    bool                                    HasSceneTree    ()const{ return !nodes_.empty(); }
    const std::vector<StaticMeshNode> &     GetNodes        ()const{ return nodes_; }
    const std::vector<int32_t> &            GetRootNodes    ()const{ return rootNodes_; }

};//class StaticMesh
}//namespace hgl::graph
