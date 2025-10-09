#include <hgl/graph/mesh/StaticMesh.h>

VK_NAMESPACE_BEGIN

StaticMesh::StaticMesh()
{
}

StaticMesh::~StaticMesh()
{

}

// Primitive 管理
Primitive *StaticMesh::CreatePrimitive(Geometry *geometry, MaterialInstance *mi, Pipeline *p)
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

    // 累积包围盒
    RefreshBoundingVolumes();
    return sm;
}

bool StaticMesh::AddSubMesh(Primitive *sm)
{
    if(!sm) return false;
    if(primitive_list.Contains(sm)) return true;

    primitive_list.Add(sm);

    // 跟踪资源
    geometry_set.Add(sm->GetGeometry());
    if (auto mi = sm->GetMaterialInstance()) mat_inst_set.Add(mi);
    if (auto pl = sm->GetPipeline())         pipeline_set.Add(pl);

    RefreshBoundingVolumes();
    return true;
}

void StaticMesh::RemoveSubMesh(Primitive *sm)
{
    if(!sm) return;

    // 先从列表删除并释放该 Mesh
    primitive_list.DeleteByValueOwn(sm);

    // 资源集合可能需要重建（避免误删共享资源复杂性）
    RebuildResourceSets();

    RefreshBoundingVolumes();
}

void StaticMesh::ClearSubMeshes()
{
    primitive_list.Clear();   // ObjectList::Clear 会负责 delete 其中的 Mesh*

    // 清空集合
    geometry_set.Clear();
    mat_inst_set.Clear();
    pipeline_set.Clear();

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

void StaticMesh::UpdateAllSubMeshes()
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
        bounding_volumes.SetFromAABB(box);
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
        if(!sm) continue;
        if (auto geometry = sm->GetGeometry())          geometry_set.Add(geometry);
        if (auto mi   = sm->GetMaterialInstance())  mat_inst_set.Add(mi);
        if (auto p    = sm->GetPipeline())          pipeline_set.Add(p);
    }
}

VK_NAMESPACE_END
