# OfflineMeshComponent 和 DirectMeshRenderer 使用说明

## 概述

这个实现提供了一个特殊的 MeshComponent 以及其配套的渲染类，它们等同于将 MaterialRenderList 中的合并渲染提前离线合成了。Mesh 那边直接提供 AssignBuffer，渲染类就不再走 MaterialRenderList，而是直接根据其提供的数据做 Instance/Indirect 渲染。

## 核心组件

### 1. OfflineMeshComponent

这是一个特殊的 MeshComponent，它包含预编译的渲染数据：

```cpp
#include<hgl/component/OfflineMeshComponent.h>

// 创建离线网格组件数据
OfflineMeshComponentData* data = new OfflineMeshComponentData(mesh);

// 设置预计算的 AssignBuffer
data->SetAssignBuffer(precomputed_assign_buffer);

// 设置实例数据
data->SetInstanceData(instance_count, first_instance);

// 设置绘制命令（可选，用于间接渲染）
data->SetDrawCommands(draw_commands, command_count);
// 或者
data->SetDrawIndexedCommands(indexed_draw_commands, command_count);

// 创建组件
ComponentDataPtr cdp(data);
OfflineMeshComponent* offline_mesh = new OfflineMeshComponent(cdp, manager);
```

### 2. DirectMeshRenderer

这是一个直接渲染器，绕过 MaterialRenderList 进行渲染：

```cpp
#include<hgl/graph/DirectMeshRenderer.h>

// 创建直接渲染器
DirectMeshRenderer renderer(vulkan_device);

// 设置相机信息
renderer.SetCameraInfo(camera_info);

// 开始渲染
renderer.Begin(command_buffer);

// 渲染单个离线网格组件
renderer.RenderDirect(offline_mesh_component);

// 或者批量渲染多个组件
OfflineMeshComponent* components[] = { comp1, comp2, comp3 };
renderer.RenderBatch(components, 3);

// 结束渲染
renderer.End();
```

## 主要优势

### 1. 性能优化
- **预编译数据**: AssignBuffer 和绘制命令都是预先计算好的，运行时无需重新计算
- **绕过批处理**: 不需要通过 MaterialRenderList 的批处理流程，直接渲染
- **减少状态切换**: 更好的状态管理，减少不必要的绑定操作

### 2. 灵活的渲染模式
- **直接渲染**: 使用传统的 vkCmdDraw/vkCmdDrawIndexed 调用
- **间接渲染**: 支持预编译的间接绘制命令
- **实例渲染**: 内置支持实例渲染参数

### 3. 离线优化
- **静态场景**: 对于静态场景，所有渲染数据可以预先计算并缓存
- **批量优化**: 可以将多个相同材质的网格预先合并处理
- **内存效率**: 减少运行时的内存分配和数据拷贝

## 使用场景

### 1. 静态场景渲染
对于大量静态几何体，可以预先计算所有渲染数据：

```cpp
// 收集所有静态网格
std::vector<MeshComponent*> static_meshes;
// ... 添加网格到列表

// 构建离线批次
OfflineBatchBuilder builder(device, material, pipeline);
for (auto* mesh : static_meshes) {
    builder.AddMeshComponent(mesh, 1); // 每个网格1个实例
}

// 生成离线网格组件
OfflineMeshComponent* static_batch = builder.BuildOfflineMesh();

// 每帧渲染时直接使用
renderer.RenderDirect(static_batch);
```

### 2. 实例化渲染
对于需要大量相同几何体的场景：

```cpp
// 创建实例化的离线组件
OfflineMeshComponentData* instance_data = new OfflineMeshComponentData(tree_mesh);
instance_data->SetAssignBuffer(tree_instances_buffer); // 包含所有树的变换数据
instance_data->SetInstanceData(1000, 0); // 1000个树实例

OfflineMeshComponent* forest = new OfflineMeshComponent(
    ComponentDataPtr(instance_data), manager);

// 一次调用渲染所有树
renderer.RenderDirect(forest);
```

### 3. LOD 优化
结合 LOD 系统使用：

```cpp
// 为不同 LOD 级别创建离线组件
OfflineMeshComponent* lod_high = CreateOfflineLOD(high_detail_meshes);
OfflineMeshComponent* lod_medium = CreateOfflineLOD(medium_detail_meshes);
OfflineMeshComponent* lod_low = CreateOfflineLOD(low_detail_meshes);

// 根据距离选择合适的 LOD
float distance = CalculateDistance(camera, object);
if (distance < 100.0f) {
    renderer.RenderDirect(lod_high);
} else if (distance < 500.0f) {
    renderer.RenderDirect(lod_medium);
} else {
    renderer.RenderDirect(lod_low);
}
```

## 实现细节

### AssignBuffer 预计算

AssignBuffer 包含每个实例的 LocalToWorld 矩阵 ID 和材质实例 ID：

```cpp
struct AssignData {
    uint16 l2w;  // LocalToWorld 矩阵索引
    uint16 mi;   // 材质实例索引
};
```

这些数据在离线阶段预先计算好，运行时直接使用。

### 状态管理

DirectMeshRenderer 维护渲染状态，避免不必要的状态切换：

- Pipeline 绑定
- 材质描述符绑定
- VAB (Vertex Attribute Buffer) 绑定
- AssignBuffer 绑定

### 错误处理

所有组件都包含完整的错误检查：

```cpp
// 检查组件是否可以渲染
if (!offline_mesh->CanRender()) {
    // 处理错误情况
    return false;
}

// 检查是否有离线数据
if (!offline_mesh->HasOfflineData()) {
    // 回退到常规渲染路径
    return false;
}
```

## 注意事项

1. **内存管理**: AssignBuffer 和绘制命令的内存由外部管理，组件不负责释放
2. **线程安全**: 当前实现不是线程安全的，如需多线程使用请添加适当的同步
3. **资源依赖**: 确保所有引用的资源（Mesh、Material、Pipeline）在渲染期间保持有效
4. **Vulkan 兼容性**: 实现基于 Vulkan API，确保目标平台支持所需的 Vulkan 特性

## 与现有系统的兼容性

这个实现与现有的 MeshComponent 和 MaterialRenderList 系统完全兼容：

- OfflineMeshComponent 继承自 MeshComponent，可以在任何使用 MeshComponent 的地方使用
- DirectMeshRenderer 是独立的渲染器，不影响现有的渲染管线
- 可以在同一场景中混合使用离线和常规渲染组件

通过这种设计，你可以逐步将性能关键的部分迁移到离线渲染系统，而不需要重写整个渲染管线。