#pragma once

#include<hgl/component/RenderComponent.h>
#include<hgl/graph/Mesh.h>
#include<hgl/math/Matrix.h>
#include<hgl/type/ArrayList.h>

VK_NAMESPACE_BEGIN
class VulkanDevice;
class VAB;
class DeviceBuffer;
VK_NAMESPACE_END

COMPONENT_NAMESPACE_BEGIN

class InstancedMeshComponent;
class InstancedMeshComponentManager;

struct InstanceData
{
    Matrix4f local_to_world;        ///<LocalToWorld变换矩阵
    MaterialInstance* material_instance;  ///<材质实例

    InstanceData()
    {
        local_to_world = Matrix4f::Identity();
        material_instance = nullptr;
    }

    InstanceData(const Matrix4f& l2w, MaterialInstance* mi)
        : local_to_world(l2w), material_instance(mi)
    {
    }
};

using InstanceDataList = ArrayList<InstanceData>;

struct InstancedMeshComponentData : public ComponentData
{
    Mesh* mesh;
    InstanceDataList instances;
    uint32_t max_instances;

public:
    InstancedMeshComponentData()
    {
        mesh = nullptr;
        max_instances = 1000; // 默认最大实例数量
    }

    InstancedMeshComponentData(Mesh* m, uint32_t max_inst = 1000)
    {
        mesh = m;
        max_instances = max_inst;
    }

    virtual ~InstancedMeshComponentData();

    COMPONENT_DATA_CLASS_BODY(InstancedMesh)
};//struct InstancedMeshComponentData

class InstancedMeshComponentManager : public ComponentManager
{
public:
    COMPONENT_MANAGER_CLASS_BODY(InstancedMesh)

public:
    InstancedMeshComponentManager() = default;

    Component* CreateComponent(ComponentDataPtr cdp) override;
    Component* CreateComponent(Mesh* mesh, uint32_t max_instances = 1000);
};//class InstancedMeshComponentManager

class InstancedMeshComponent : public RenderComponent
{
    WeakPtr<ComponentData> sm_data;
    
    // 直接管理自己的缓冲区，而不是使用RenderAssignBuffer
    hgl::graph::VulkanDevice* device;
    hgl::graph::VAB* instance_vab;           // 实例数据VAB
    hgl::graph::DeviceBuffer* l2w_buffer;    // LocalToWorld矩阵缓冲区
    hgl::graph::DeviceBuffer* mi_buffer;     // 材质实例缓冲区
    bool buffers_dirty;

public:
    COMPONENT_CLASS_BODY(InstancedMesh)

public:
    InstancedMeshComponent(ComponentDataPtr cdp, InstancedMeshComponentManager* cm);
    virtual ~InstancedMeshComponent();

    virtual Component* Duplication() override;

    InstancedMeshComponentData* GetData() { return dynamic_cast<InstancedMeshComponentData*>(sm_data.get()); }
    const InstancedMeshComponentData* GetData() const { return dynamic_cast<const InstancedMeshComponentData*>(sm_data.const_get()); }

    Mesh* GetMesh() const;
    const bool GetLocalAABB(AABB& box) const override;

public:
    // 实例管理方法
    bool AddInstance(const Matrix4f& local_to_world, MaterialInstance* material_instance);
    bool RemoveInstance(uint32_t index);
    bool UpdateInstance(uint32_t index, const Matrix4f& local_to_world, MaterialInstance* material_instance = nullptr);
    void ClearInstances();
    uint32_t GetInstanceCount() const;

    // 渲染相关方法
    Pipeline* GetPipeline() const;
    Material* GetMaterial() const;
    const bool CanRender() const override;

    // Buffer管理
    void SetVulkanDevice(hgl::graph::VulkanDevice* dev);
    bool UpdateBuffers();
    hgl::graph::VAB* GetInstanceVAB() const { return instance_vab; }

private:
    void InitializeBuffers();
    void CleanupBuffers();
};//class InstancedMeshComponent

COMPONENT_NAMESPACE_END