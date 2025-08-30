#include<hgl/component/InstancedMeshComponent.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKVertexAttribBuffer.h>

COMPONENT_NAMESPACE_BEGIN

InstancedMeshComponentData::~InstancedMeshComponentData()
{
    // 清理资源
}

InstancedMeshComponent::InstancedMeshComponent(ComponentDataPtr cdp, InstancedMeshComponentManager* cm)
    : RenderComponent(cdp, cm)
{
    sm_data = cdp;
    device = nullptr;
    instance_vab = nullptr;
    l2w_buffer = nullptr;
    mi_buffer = nullptr;
    buffers_dirty = true;
}

InstancedMeshComponent::~InstancedMeshComponent()
{
    CleanupBuffers();
}

Component* InstancedMeshComponent::Duplication()
{
    InstancedMeshComponent* imc = (InstancedMeshComponent*)RenderComponent::Duplication();

    if (!imc)
        return nullptr;

    // 复制实例数据
    if (buffers_dirty)
        imc->buffers_dirty = true;

    return imc;
}

Mesh* InstancedMeshComponent::GetMesh() const
{
    if (!sm_data.valid())
        return nullptr;

    const InstancedMeshComponentData* imcd = dynamic_cast<const InstancedMeshComponentData*>(sm_data.const_get());

    if (!imcd)
        return nullptr;

    return imcd->mesh;
}

const bool InstancedMeshComponent::GetLocalAABB(AABB& box) const
{
    Mesh* mesh = GetMesh();

    if (!mesh)
        return false;

    box = mesh->GetBoundingBox();
    return true;
}

bool InstancedMeshComponent::AddInstance(const Matrix4f& local_to_world, MaterialInstance* material_instance)
{
    InstancedMeshComponentData* data = GetData();
    if (!data)
        return false;

    if (data->instances.GetCount() >= data->max_instances)
        return false;

    data->instances.Add(InstanceData(local_to_world, material_instance));
    buffers_dirty = true;
    return true;
}

bool InstancedMeshComponent::RemoveInstance(uint32_t index)
{
    InstancedMeshComponentData* data = GetData();
    if (!data)
        return false;

    if (index >= data->instances.GetCount())
        return false;

    data->instances.DeleteByIndex(index);
    buffers_dirty = true;
    return true;
}

bool InstancedMeshComponent::UpdateInstance(uint32_t index, const Matrix4f& local_to_world, MaterialInstance* material_instance)
{
    InstancedMeshComponentData* data = GetData();
    if (!data)
        return false;

    if (index >= data->instances.GetCount())
        return false;

    InstanceData& inst = data->instances[index];
    inst.local_to_world = local_to_world;
    if (material_instance)
        inst.material_instance = material_instance;

    buffers_dirty = true;
    return true;
}

void InstancedMeshComponent::ClearInstances()
{
    InstancedMeshComponentData* data = GetData();
    if (data)
    {
        data->instances.Clear();
        buffers_dirty = true;
    }
}

uint32_t InstancedMeshComponent::GetInstanceCount() const
{
    const InstancedMeshComponentData* data = GetData();
    if (!data)
        return 0;

    return data->instances.GetCount();
}

Pipeline* InstancedMeshComponent::GetPipeline() const
{
    Mesh* mesh = GetMesh();

    if (!mesh)
        return nullptr;

    return mesh->GetPipeline();
}

Material* InstancedMeshComponent::GetMaterial() const
{
    Mesh* mesh = GetMesh();

    if (!mesh)
        return nullptr;

    return mesh->GetMaterial();
}

const bool InstancedMeshComponent::CanRender() const
{
    if (!sm_data.valid())
        return false;

    const InstancedMeshComponentData* data = GetData();

    if (!data || !data->mesh)
        return false;

    return data->instances.GetCount() > 0;
}

void InstancedMeshComponent::SetVulkanDevice(hgl::graph::VulkanDevice* dev)
{
    device = dev;
    if (device && !instance_vab)
    {
        InitializeBuffers();
    }
}

void InstancedMeshComponent::InitializeBuffers()
{
    if (!device)
        return;

    const InstancedMeshComponentData* data = GetData();
    if (!data)
        return;

    // 创建实例数据VAB (RG16UI格式：R存L2W ID，G存材质实例ID)
    // 暂时创建一个简单的VAB，实际的格式可能需要根据材质要求调整
    if (!instance_vab)
    {
        instance_vab = device->CreateVAB(VK_FORMAT_R32G32_UINT, data->max_instances);
    }

    buffers_dirty = true;
}

void InstancedMeshComponent::CleanupBuffers()
{
    if (instance_vab)
    {
        delete instance_vab;
        instance_vab = nullptr;
    }
    if (l2w_buffer)
    {
        delete l2w_buffer;
        l2w_buffer = nullptr;
    }
    if (mi_buffer)
    {
        delete mi_buffer;
        mi_buffer = nullptr;
    }
}

bool InstancedMeshComponent::UpdateBuffers()
{
    if (!buffers_dirty || !device)
        return true;

    const InstancedMeshComponentData* data = GetData();
    if (!data || data->instances.GetCount() == 0)
        return false;

    // 这里应该更新实例数据到VAB
    // 具体实现需要根据材质的顶点输入格式来调整
    // 暂时标记为非dirty状态
    buffers_dirty = false;
    
    return true;
}

// InstancedMeshComponentManager implementation
Component* InstancedMeshComponentManager::CreateComponent(ComponentDataPtr cdp)
{
    return new InstancedMeshComponent(cdp, this);
}

Component* InstancedMeshComponentManager::CreateComponent(Mesh* mesh, uint32_t max_instances)
{
    InstancedMeshComponentData* data = new InstancedMeshComponentData(mesh, max_instances);
    ComponentDataPtr cdp(data);
    return CreateComponent(cdp);
}

COMPONENT_NAMESPACE_END