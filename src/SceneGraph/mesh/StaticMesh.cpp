#include <hgl/graph/mesh/StaticMesh.h>
#include <unordered_map>

namespace hgl::graph{

StaticMesh::StaticMesh()
{
}

StaticMesh::~StaticMesh()
{
}

// Primitive 管理
Primitive *StaticMesh::CreatePrimitive(Geometry *geometry, MaterialInstance *mi, GraphicsPipeline *p)
{
    if(!geometry || !mi || !p)
        return nullptr;

    Primitive *sm = DirectCreatePrimitive(geometry, mi, p);
    if(!sm)
        return nullptr;

    primitive_list.Add(sm);

    // 跟踪资源
    geometry_set.Add(geometry);
    mat_inst_set.Add(mi);
    pipeline_set.Add(p);

    // 缓存 Primitive -> Pipeline 映射
    _primitive_pipeline_cache[sm] = p;

    // 累积包围盒
    RefreshBoundingVolumes();
    return sm;
}

bool StaticMesh::AddPrimitive(Primitive *sm)
{
    if(!sm) return false;
    if(primitive_list.Contains(sm)) return true;

    primitive_list.Add(sm);

    // 跟踪资源
    geometry_set.Add(sm->GetGeometry());
    if (auto mi = sm->GetMaterialInstance()) mat_inst_set.Add(mi);

    // 处理 Pipeline 缓存：
    // 外部导入的 Primitive 可能没有 pipeline 信息（已完全解耦），
    // 所以缓存初始为空，不从 Primitive 读取。
    // 如需要可在将来指定。
    _primitive_pipeline_cache[sm] = nullptr;

    RefreshBoundingVolumes();
    return true;
}

void StaticMesh::RemovePrimitive(Primitive *sm)
{
    if(!sm) return;

    // 先从列表删除并释放该 Mesh
    primitive_list.DeleteByValue(sm);

    // 从 Pipeline 缓存中移除
    _primitive_pipeline_cache.erase(sm);

    // 资源集合可能需要重建（避免误删共享资源复杂性）
    RebuildResourceSets();

    RefreshBoundingVolumes();
}

void StaticMesh::ClearPrimitives()
{
    primitive_list.Clear();   // ManagedArray::Clear 会负责 delete 其中的 Mesh*

    // 清空集合和缓存
    geometry_set.Clear();
    mat_inst_set.Clear();
    pipeline_set.Clear();
    _primitive_pipeline_cache.clear();

    bounding_volumes.Clear();
}

bool StaticMesh::AttachGeometry(Geometry *geometry)
{
    if(!geometry) return false;
    return geometry_set.Add(geometry) >= 0;
}

void StaticMesh::DetachGeometry(Geometry *geometry)
{
    if(!geometry) return;
    geometry_set.Delete(geometry);
}

void StaticMesh::UpdatePrimitives()
{
    for(Primitive *sm: primitive_list)
        if(sm) sm->UpdateGeometry();
}

void StaticMesh::RefreshBoundingVolumes()
{
    bool has_box = false;
    AABB box;

    for(Primitive *sm: primitive_list)
    {
        if(!sm)
            continue;

        if(!has_box)
        {
            box = sm->GetBoundingVolumes().aabb;

            has_box = true;
        }
        else
        {
            box.Merge(sm->GetBoundingVolumes().aabb);
        }
    }

    if(has_box)
        this->bounding_volumes.SetFromAABB(box);
    else
        bounding_volumes.Clear();
}

void StaticMesh::RebuildResourceSets()
{
    geometry_set.Clear();
    mat_inst_set.Clear();
    pipeline_set.Clear();

    for(Primitive *sm : primitive_list)
    {
        if(!sm)
            continue;

        if (auto geom = sm->GetGeometry())          geometry_set.Add(geom);
        if (auto mi   = sm->GetMaterialInstance())  mat_inst_set.Add(mi);

        // 从缓存中获取 Pipeline，而不是调用 GetPipeline()
        auto it = _primitive_pipeline_cache.find(sm);
        if (it != _primitive_pipeline_cache.end() && it->second)
        {
            pipeline_set.Add(it->second);
        }
    }
}

int32_t StaticMesh::AddNode(StaticMeshNode &&node)
{
    const int32_t idx = static_cast<int32_t>(nodes_.size());
    nodes_.push_back(std::move(node));
    return idx;
}

}//namespace hgl::graph
