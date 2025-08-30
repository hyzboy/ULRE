/**
 * InstancedMeshComponent 使用示例
 * 
 * 这个示例展示了如何使用新的InstancedMeshComponent来替代MaterialRenderList
 * 进行更直接的实例化渲染控制
 */

#include<hgl/component/InstancedMeshComponent.h>
#include<hgl/graph/InstancedMeshRenderer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/SceneNode.h>

using namespace hgl::graph;

/**
 * 示例：创建并使用InstancedMeshComponent
 */
void InstancedMeshComponentExample(VulkanDevice* device, Mesh* mesh)
{
    // 1. 创建InstancedMeshComponentManager
    InstancedMeshComponentManager* manager = InstancedMeshComponentManager::GetDefaultManager();

    // 2. 创建InstancedMeshComponent (最大支持100个实例)
    Component* comp = manager->CreateComponent(mesh, 100);
    InstancedMeshComponent* instanced_comp = dynamic_cast<InstancedMeshComponent*>(comp);
    
    if (!instanced_comp)
        return;

    // 3. 设置VulkanDevice用于创建GPU缓冲区
    instanced_comp->SetVulkanDevice(device);

    // 4. 添加实例数据
    Matrix4f transform1 = Matrix4f::Identity();
    transform1.SetTranslate(Vector3f(1.0f, 0.0f, 0.0f));
    
    Matrix4f transform2 = Matrix4f::Identity();
    transform2.SetTranslate(Vector3f(-1.0f, 0.0f, 0.0f));
    
    Matrix4f transform3 = Matrix4f::Identity();
    transform3.SetTranslate(Vector3f(0.0f, 1.0f, 0.0f));

    // 添加三个实例，使用相同的材质实例（也可以使用不同的材质实例）
    MaterialInstance* material_instance = mesh->GetMaterialInstance();
    instanced_comp->AddInstance(transform1, material_instance);
    instanced_comp->AddInstance(transform2, material_instance);
    instanced_comp->AddInstance(transform3, material_instance);

    // 5. 创建专用的渲染器
    InstancedMeshRenderer* renderer = new InstancedMeshRenderer(device);

    // 6. 渲染 (在渲染循环中调用)
    // RenderCmdBuffer* cmd_buffer = ...; // 从渲染框架获取
    // renderer->Render(instanced_comp, cmd_buffer);

    // 7. 更新实例数据 (可选)
    Matrix4f new_transform = Matrix4f::Identity();
    new_transform.SetTranslate(Vector3f(2.0f, 0.0f, 0.0f));
    instanced_comp->UpdateInstance(0, new_transform); // 更新第一个实例的位置

    // 8. 删除实例 (可选)
    // instanced_comp->RemoveInstance(1); // 删除第二个实例

    // 9. 清理资源
    delete renderer;
    delete instanced_comp;
}

/**
 * 对比传统的MaterialRenderList使用方式
 */
void TraditionalMaterialRenderListExample(VulkanDevice* device, Mesh* mesh)
{
    // 传统方式：需要创建多个MeshComponent，然后通过MaterialRenderList管理
    // 1. 创建多个SceneNode和MeshComponent
    // 2. 将它们添加到MaterialRenderList
    // 3. MaterialRenderList内部管理AssignBuffer
    // 4. 间接渲染，用户控制较少

    // 新的InstancedMeshComponent方式：
    // 1. 直接创建一个InstancedMeshComponent
    // 2. 直接管理实例数据和AssignBuffer
    // 3. 使用专用的InstancedMeshRenderer进行渲染
    // 4. 更直接的控制，更手动的管理
}

/**
 * 注意事项和限制：
 * 
 * 1. InstancedMeshComponent直接管理自己的AssignBuffer，不再通过MaterialRenderList
 * 2. 需要使用专用的InstancedMeshRenderer进行渲染，不能与MaterialRenderList混用
 * 3. 每个InstancedMeshComponent只能使用一个Mesh，但可以有多个不同的MaterialInstance
 * 4. 实例数据的格式需要与材质的顶点输入格式匹配
 * 5. 需要手动管理实例数据的更新和同步
 * 
 * 优势：
 * 1. 更直接的控制实例化渲染过程
 * 2. 不依赖MaterialRenderList的自动化管理
 * 3. 可以更灵活地处理大量实例的场景
 * 4. 减少了MaterialRenderList的复杂性和间接性
 */