#include <hgl/graph/mesh/StaticMesh.h>

VK_NAMESPACE_BEGIN

StaticMesh::StaticMesh()
{
    // 创建一个根节点，Mesh 持有
    root_node = nodes.Create();
}

StaticMesh::~StaticMesh()
{
    ClearSubMeshes();
    ClearNodes();
}

// MeshNode 管理
void StaticMesh::RemoveNode(MeshNode *node)
{
    if(!node) return;

    if(node == root_node)
    {
        // 清空所有并重建根
        ClearNodes();
        return;
    }

    // nodes 为 ObjectList 拥有：删除并销毁
    nodes.DeleteByValueOwn(node);
}

void StaticMesh::ClearNodes()
{
    nodes.Clear();
    // 重建一个空根节点
    root_node = nodes.Create();
}

// SubMesh 管理
SubMesh *StaticMesh::CreateSubMesh(Primitive *prim, MaterialInstance *mi, Pipeline *p)
{
    if(!prim || !mi || !p)
        return nullptr;

    SubMesh *sm = DirectCreateSubMesh(prim, mi, p);
    if(!sm)
        return nullptr;

    submeshes.Add(sm);

    // 跟踪资源
    primitives.Add(prim);
    mat_inst_set.Add(mi);
    pipeline_set.Add(p);

    // 累积包围盒
    RefreshBoundingBox();
    return sm;
}

bool StaticMesh::AddSubMesh(SubMesh *sm)
{
    if(!sm) return false;
    if(submeshes.Contains(sm)) return true;

    submeshes.Add(sm);

    // 跟踪资源
    primitives.Add(sm->GetPrimitive());
    if (auto mi = sm->GetMaterialInstance()) mat_inst_set.Add(mi);
    if (auto pl = sm->GetPipeline())         pipeline_set.Add(pl);

    RefreshBoundingBox();
    return true;
}

void StaticMesh::RemoveSubMesh(SubMesh *sm)
{
    if(!sm) return;

    // 先从列表删除并释放该 SubMesh
    submeshes.DeleteByValueOwn(sm);

    // 资源集合可能需要重建（避免误删共享资源复杂性）
    RebuildResourceSets();

    RefreshBoundingBox();
}

void StaticMesh::ClearSubMeshes()
{
    submeshes.Clear();   // ObjectList::Clear 会负责 delete 其中的 SubMesh*

    // 清空集合
    primitives.Clear();
    mat_inst_set.Clear();
    pipeline_set.Clear();

    local_bounding_box.Clear();
}

bool StaticMesh::AttachPrimitive(Primitive *prim)
{
    if(!prim) return false;
    return primitives.Add(prim) >= 0;
}

void StaticMesh::DetachPrimitive(Primitive *prim)
{
    if(!prim) return;
    primitives.Delete(prim);
}

void StaticMesh::UpdateAllSubMeshes()
{
    for(SubMesh *sm: submeshes)
        if(sm) sm->UpdatePrimitive();
}

void StaticMesh::RefreshBoundingBox()
{
    bool has_box = false;
    AABB box;

    for(SubMesh *sm: submeshes)
    {
        if(!sm) continue;
        if(!has_box) { box = sm->GetBoundingBox(); has_box = true; }
        else { box.Enclose(sm->GetBoundingBox()); }
    }

    if(has_box) local_bounding_box = box; else local_bounding_box.Clear();
}

void StaticMesh::RebuildResourceSets()
{
    primitives.Clear();
    mat_inst_set.Clear();
    pipeline_set.Clear();

    for(SubMesh *sm : submeshes)
    {
        if(!sm) continue;
        if (auto prim = sm->GetPrimitive())          primitives.Add(prim);
        if (auto mi   = sm->GetMaterialInstance())   mat_inst_set.Add(mi);
        if (auto p    = sm->GetPipeline())           pipeline_set.Add(p);
    }
}

VK_NAMESPACE_END
