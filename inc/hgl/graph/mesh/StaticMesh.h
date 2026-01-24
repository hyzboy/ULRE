#pragma once

#include <hgl/type/ObjectList.h>
#include <hgl/type/SortedSet.h>
#include <hgl/type/ValueArray.h>
#include <hgl/graph/mesh/Primitive.h>

VK_NAMESPACE_BEGIN

using GeometryPtrSet        =SortedSet<Geometry *>;
using MaterialInstanceSet   =SortedSet<MaterialInstance *>;
using PipelinePtrSet        =SortedSet<Pipeline *>;
using PrimitiveList         =ObjectList<Primitive>;

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

public:

    StaticMesh();
    virtual ~StaticMesh();

public: // Geometry / MaterialInstance / Pipeline(仅保存引用,便于统计/查询)

    bool                        AttachGeometry      (Geometry *geometry);
    void                        DetachGeometry      (Geometry *geometry);
    const GeometryPtrSet &      GetGeometries       () const { return geometry_set; }

    const MaterialInstanceSet & GetMaterialInstances() const { return mat_inst_set; }
    const PipelinePtrSet &      GetPipelines        () const { return pipeline_set; }

public: // Primitive 管理

    const int                   GetPrimitiveCount   ()const{ return primitive_list.GetCount(); }
    const PrimitiveList &       GetPrimitiveList    ()const{ return primitive_list; }

    Primitive *                 CreatePrimitive     (Geometry *geometry, MaterialInstance *mi, Pipeline *p);            ///< 创建并添加一个 Primitive(为该 Primitive 指定 Geometry / MaterialInstance / Pipeline)

    bool                        AddPrimitive        (Primitive *sm);                                                    ///< 添加一个已有的 Primitive(StaticMesh 将接管其生命周期)

    void                        RemovePrimitive     (Primitive *sm);                                                    ///< 从 StaticMesh 中移除并销毁一个 Primitive

    void                        ClearPrimitives     ();                                                                 ///< 清空并销毁所有 Primitive

    void                        UpdatePrimitives  ();                                                                   ///< 当 Geometry/VIL 数据发生变化时,更新所有 Primitive 的渲染数据

public: // 包围盒

    void                        RefreshBoundingVolumes  ();
    const BoundingVolumes &     GetBoundingVolumes      () const { return bounding_volumes; }

private:

    void                        RebuildResourceSets ();
};//class StaticMesh
VK_NAMESPACE_END
