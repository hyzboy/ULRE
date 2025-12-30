# ECS Rendering Implementation

## 概述 / Overview

实现了 ECS 系统的实际渲染功能，参考 `inc/hgl/graph` 中的 `PipelineMaterialBatch` 和 `DrawNode` 实现。

Implemented actual rendering functionality for ECS systems, referencing `PipelineMaterialBatch` and `DrawNode` from `inc/hgl/graph`.

## 实现的渲染方法 / Implemented Rendering Methods

### MaterialBatch::Render()

负责渲染单个材质批次中的所有项目。

Renders all items in a single material batch.

**实现步骤 / Implementation Steps:**
1. 绑定管线 (Bind Pipeline)
2. 绑定材质描述符集 (Bind Material Descriptor Sets)
3. 遍历批次中的所有渲染项 (Iterate through batch items)
4. 对每个项目：
   - 绑定顶点缓冲区 (Bind vertex buffers)
   - 绑定索引缓冲区（如果有）(Bind index buffer if present)
   - 发出绘制命令 (Issue draw command)

**代码示例 / Code Example:**
```cpp
void MaterialBatch::Render(graph::RenderCmdBuffer* cmdBuffer)
{
    // Bind pipeline once for entire batch
    cmdBuffer->BindPipeline(key.pipeline);
    
    // Bind material descriptor sets once
    cmdBuffer->BindDescriptorSets(key.material);
    
    // Render each item
    for (RenderItem* item : items)
    {
        auto* primitive = item->GetPrimitive();
        auto* dataBuffer = primitive->GetDataBuffer();
        auto* drawRange = primitive->GetRenderData();
        
        // Bind geometry and draw
        if (dataBuffer->ibo)
            cmdBuffer->BindIBO(dataBuffer->ibo);
            
        cmdBuffer->Draw(dataBuffer, drawRange, 1, item->transform_index);
    }
}
```

### RenderPrimitiveSystem::Render()

渲染所有收集到的 PrimitiveComponent。

Renders all collected PrimitiveComponents.

**实现步骤 / Implementation Steps:**
1. 检查命令缓冲区和渲染项数量
2. 遍历所有材质批次
3. 对每个批次调用 `MaterialBatch::Render()`

**代码示例 / Code Example:**
```cpp
void RenderPrimitiveSystem::Render(graph::RenderCmdBuffer* cmdBuffer)
{
    if (!cmdBuffer || renderableCount == 0)
        return;
    
    // Render each material batch
    for (auto& pair : materialBatches)
    {
        MaterialBatch* batch = pair.second.get();
        if (batch && batch->GetCount() > 0)
        {
            batch->Render(cmdBuffer);
        }
    }
}
```

### RenderCollector::Render()

通用渲染器的渲染方法，与 RenderPrimitiveSystem 类似。

Generic collector's render method, similar to RenderPrimitiveSystem.

## 与旧系统的对比 / Comparison with Old System

### 旧系统 (Old - inc/hgl/graph/PipelineMaterialBatch)

```cpp
class PipelineMaterialBatch
{
    void Render(RenderCmdBuffer *rcb)
    {
        cmd_buf = rcb;
        
        // Bind pipeline
        cmd_buf->BindPipeline(pm_index.pipeline);
        
        // Handle transform/material instance buffers
        if (assign_buffer)
            assign_buffer->Bind(pm_index.material);
            
        // Bind descriptor sets
        cmd_buf->BindDescriptorSets(pm_index.material);
        
        // Render batches
        for (uint i = 0; i < draw_batches_count; i++)
        {
            Draw(batch);
            ++batch;
        }
        
        // Handle indirect rendering
        if (indirect_draw_count)
            ProcIndirectRender();
    }
};
```

### 新系统 (New - inc/hgl/ecs/MaterialBatch)

```cpp
class MaterialBatch
{
    void Render(graph::RenderCmdBuffer* cmdBuffer)
    {
        // Bind pipeline
        cmdBuffer->BindPipeline(key.pipeline);
        
        // Bind material descriptor sets
        cmdBuffer->BindDescriptorSets(key.material);
        
        // Render each item (simplified approach)
        for (RenderItem* item : items)
        {
            auto* primitive = item->GetPrimitive();
            auto* dataBuffer = primitive->GetDataBuffer();
            auto* drawRange = primitive->GetRenderData();
            
            if (dataBuffer->ibo)
                cmdBuffer->BindIBO(dataBuffer->ibo);
                
            cmdBuffer->Draw(dataBuffer, drawRange, 1, item->transform_index);
        }
    }
};
```

## 主要差异 / Key Differences

### 简化的实现 / Simplified Implementation

新的 ECS 实现采用了更简单直接的方法：

The new ECS implementation uses a more straightforward approach:

1. **无间接绘制优化** - 当前实现直接绘制每个项目
   - **No indirect draw optimization** - Current implementation draws each item directly

2. **简化的 VAB 管理** - 不使用复杂的 VABList
   - **Simplified VAB management** - Doesn't use complex VABList

3. **无实例分配缓冲区** - 不使用 InstanceAssignmentBuffer
   - **No instance assignment buffer** - Doesn't use InstanceAssignmentBuffer

### 未来优化 / Future Optimizations

可以添加以下优化以接近旧系统的性能：

The following optimizations can be added to approach the old system's performance:

1. **间接绘制支持** - Indirect Draw Support
   - 批量提交绘制命令
   - Batch draw command submission

2. **实例化渲染** - Instanced Rendering
   - 合并相同几何体的多个实例
   - Merge multiple instances of same geometry

3. **变换数据缓冲区** - Transform Data Buffer
   - 统一管理所有变换矩阵
   - Unified management of all transform matrices

4. **材质实例缓冲区** - Material Instance Buffer
   - 支持材质实例数据的批量更新
   - Support batch updates of material instance data

## 使用示例 / Usage Example

### 设置和渲染 / Setup and Rendering

```cpp
// 1. Create and initialize system
auto renderSystem = world->RegisterSystem<RenderPrimitiveSystem>();
renderSystem->SetWorld(world);
renderSystem->SetDevice(vulkanDevice);  // NEW: Set Vulkan device
renderSystem->Initialize();

// 2. Set camera
CameraInfo camera;
// ... configure camera
renderSystem->SetCameraInfo(&camera);

// 3. Create entities with PrimitiveComponent
auto entity = world->CreateEntity<Entity>("MeshObject");
auto transform = entity->AddComponent<TransformComponent>();
auto primitiveComp = entity->AddComponent<PrimitiveComponent>();
// ... set primitive data

// 4. Collect primitives
renderSystem->CollectPrimitives();

// 5. Render to command buffer (NEW)
RenderCmdBuffer* cmdBuffer = ...; // Get command buffer from render pass
renderSystem->Render(cmdBuffer);  // Actually submit rendering commands
```

### 完整渲染循环 / Complete Rendering Loop

```cpp
void GameLoop()
{
    // Update phase
    world->Update(deltaTime);
    
    // Rendering phase
    auto* cmdBuffer = renderContext->BeginFrame();
    
    // Begin render pass
    cmdBuffer->BeginRenderPass(renderPass);
    
    // Collect and render primitives
    auto* renderSystem = world->GetSystem<RenderPrimitiveSystem>();
    renderSystem->CollectPrimitives();
    renderSystem->Render(cmdBuffer);  // Submit draw commands
    
    // End render pass
    cmdBuffer->EndRenderPass();
    
    // Submit and present
    renderContext->EndFrame();
}
```

## API 变更总结 / API Changes Summary

### 新增方法 / New Methods

**MaterialBatch:**
- `void Render(graph::RenderCmdBuffer* cmdBuffer)` - 渲染批次
- `void SetDevice(graph::VulkanDevice* dev)` - 设置设备
- Constructor 现在接受 `VulkanDevice*` 参数

**RenderPrimitiveSystem:**
- `void SetDevice(graph::VulkanDevice* dev)` - 设置 Vulkan 设备
- `void Render(graph::RenderCmdBuffer* cmdBuffer)` - 渲染所有原始组件

**RenderCollector:**
- `void SetDevice(graph::VulkanDevice* dev)` - 设置 Vulkan 设备
- `void Render(graph::RenderCmdBuffer* cmdBuffer)` - 渲染所有收集的项目

### 必需的初始化步骤 / Required Initialization Steps

```cpp
// Before (只收集)
renderSystem->CollectPrimitives();
const auto& batches = renderSystem->GetMaterialBatches();
// User manually renders...

// After (自动渲染)
renderSystem->SetDevice(device);  // NEW: Required
renderSystem->CollectPrimitives();
renderSystem->Render(cmdBuffer);   // NEW: Automatic rendering
```

## 性能考虑 / Performance Considerations

### 当前实现 / Current Implementation

- ✅ 按材质/管线批处理 - Material/Pipeline batching
- ✅ 几何体排序 - Geometry sorting
- ✅ 减少状态切换 - Reduced state changes
- ⚠️ 每个对象单独绘制 - Individual draw per object

### 未来改进 / Future Improvements

- 🔄 间接绘制批处理 - Indirect draw batching
- 🔄 实例化渲染 - Instanced rendering
- 🔄 GPU 实例缓冲区 - GPU instance buffers
- 🔄 持久化映射缓冲区 - Persistent mapped buffers

## 参考 / References

**旧系统实现 / Old System Implementation:**
- `inc/hgl/graph/PipelineMaterialBatch.h`
- `src/SceneGraph/render/PipelineMaterialBatch.cpp`
- `inc/hgl/graph/DrawNode.h`
- `src/SceneGraph/render/DrawNode.cpp`

**新系统实现 / New System Implementation:**
- `inc/hgl/ecs/MaterialBatch.h`
- `src/ecs/MaterialBatch.cpp`
- `inc/hgl/ecs/RenderPrimitiveSystem.h`
- `src/ecs/RenderPrimitiveSystem.cpp`
- `inc/hgl/ecs/RenderCollector.h`
- `src/ecs/RenderCollector.cpp`
