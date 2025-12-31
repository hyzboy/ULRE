# PipelineMaterialBatch 架构拆分说明

## 概述

根据 @hyzboy 的要求，将 `PipelineMaterialBatch` 的功能拆分为两个独立的类：
- **PipelineMaterialBatch** - 负责收集整理
- **PipelineMaterialRenderer** - 负责渲染执行

---

## 架构变更

### 拆分前 (单一类)

```
PipelineMaterialBatch
  ├─ 节点管理 (Add, Clear, SetCameraInfo)
  ├─ 批次构建 (BuildBatches, Finalize)
  ├─ 数据更新 (UpdateTransformData, UpdateMaterialInstanceData)
  ├─ 渲染执行 (Render, Draw, BindVAB, ProcIndirectRender)
  └─ 状态管理 (cmd_buf, vab_list, last_*, indirect_draw_*)
```

### 拆分后 (两个独立类)

```
PipelineMaterialBatch (收集整理)
  ├─ 节点收集 (Add, Clear, SetCameraInfo)
  ├─ 节点排序 (Finalize中调用Sort)
  ├─ 批次构建 (BuildBatches)
  ├─ 数据更新 (UpdateTransformData, UpdateMaterialInstanceData)
  ├─ ICB分配 (ReallocICB, WriteICB)
  └─ 渲染委托 (Render -> 调用renderer->Render)

PipelineMaterialRenderer (渲染执行)
  ├─ 渲染命令 (Render)
  ├─ VAB绑定 (BindVAB)
  ├─ 绘制批次 (Draw)
  ├─ 间接绘制 (ProcIndirectRender)
  └─ 状态缓存 (cmd_buf, vab_list, last_*, indirect_draw_*)
```

---

## 职责划分

### PipelineMaterialBatch (数据组织)

**核心职责：**
- 收集和管理使用相同 Material 和 Pipeline 的渲染节点
- 对节点进行排序以优化渲染顺序
- 构建绘制批次（合并相同几何数据的节点）
- 管理实例数据（LocalToWorld 矩阵、材质实例数据）
- 分配和填充间接绘制命令缓冲

**拥有的数据：**
```cpp
// 核心标识
VulkanDevice *device;
PipelineMaterialIndex pm_index;
const CameraInfo *camera_info;

// 节点管理
DrawNodeList draw_nodes;
DrawNodePointerList transform_dirty_nodes;

// 实例数据
InstanceAssignmentBuffer *assign_buffer;

// 批次数据
DrawBatchArray draw_batches;
uint draw_batches_count;

// 间接绘制缓冲
IndirectDrawBuffer *icb_draw;
IndirectDrawIndexedBuffer *icb_draw_indexed;

// 渲染器
PipelineMaterialRenderer *renderer;
```

**主要方法：**
- `Add(DrawNode*)` - 添加渲染节点
- `Clear()` - 清空所有节点
- `SetCameraInfo()` - 设置相机信息
- `Finalize()` - 排序并构建批次
- `BuildBatches()` - 构建绘制批次
- `UpdateTransformData()` - 更新变换数据
- `UpdateMaterialInstanceData()` - 更新材质实例数据
- `Render()` - 委托给renderer执行渲染

---

### PipelineMaterialRenderer (渲染执行)

**核心职责：**
- 执行实际的渲染命令
- 管理渲染状态（VAB绑定、IBO绑定）
- 处理直接绘制和间接绘制
- 优化状态切换（缓存上次绑定的资源）

**拥有的数据：**
```cpp
// 核心标识
Material *material;
Pipeline *pipeline;

// 渲染状态
RenderCmdBuffer *cmd_buf;
VABList *vab_list;

// 状态缓存（优化状态切换）
const GeometryDataBuffer *last_data_buffer;
const VDM *last_vdm;
const GeometryDrawRange *last_draw_range;

// 间接绘制状态
int first_indirect_draw_index;
uint indirect_draw_count;
```

**主要方法：**
- `Render()` - 执行渲染
- `BindVAB()` - 绑定顶点属性缓冲
- `Draw()` - 绘制单个批次
- `ProcIndirectRender()` - 处理累积的间接绘制命令

**Render方法参数：**
```cpp
void Render(RenderCmdBuffer *rcb,
            const DrawBatchArray &batches,
            uint batch_count,
            InstanceAssignmentBuffer *assign_buffer,
            IndirectDrawBuffer *icb_draw,
            IndirectDrawIndexedBuffer *icb_draw_indexed);
```

---

## 数据流程

### 1. 收集阶段
```
RenderCollector::Expand()
  └─> RenderBatchMap::Begin()  // 清空现有节点
      └─> batch->SetCameraInfo()
      └─> batch->Clear()
  
  └─> Component::SubmitDrawNodes()
      └─> RenderBatchMap::AddDrawNode()
          └─> batch->Add(node)  // 添加节点到批次
```

### 2. 构建阶段
```
RenderCollector::Expand()
  └─> RenderBatchMap::End()
      └─> batch->Finalize()
          ├─> Sort(draw_nodes)      // 排序节点
          ├─> BuildBatches()        // 构建批次
          │   ├─> ReallocICB()      // 分配ICB
          │   ├─> 合并相同几何数据的节点
          │   └─> WriteICB()        // 写入间接绘制命令
          └─> assign_buffer->WriteNode()  // 写入实例数据
```

### 3. 渲染阶段
```
RenderCollector::Render()
  └─> RenderBatchMap::Render()
      └─> batch->Render(cmd_buf)
          └─> renderer->Render(cmd_buf, batches, ...)
              ├─> 绑定管线和材质
              ├─> 遍历批次
              │   ├─> BindVAB()      // 绑定VAB
              │   ├─> Draw()         // 绘制或累积间接绘制
              │   └─> 状态缓存优化
              └─> ProcIndirectRender()  // 提交累积的间接绘制
```

---

## 关键改进

### 1. 关注点分离
- **收集阶段** 专注于数据组织和优化
- **渲染阶段** 专注于命令执行
- 两个阶段可以独立优化和测试

### 2. 状态管理
- 渲染状态（cmd_buf, vab_list, last_*）只在渲染器中
- 收集状态（draw_nodes, batches）只在批次管理器中
- 避免了状态混乱

### 3. 批次构建优化
- `BuildBatches()` 使用局部变量缓存几何信息
- 不再依赖渲染状态变量
- 批次构建和渲染完全解耦

### 4. 接口简洁性
- `PipelineMaterialBatch::Render()` 简化为一次委托调用
- `PipelineMaterialRenderer::Render()` 接收所有需要的数据
- 无需在渲染器和批次管理器间共享状态

---

## 向后兼容性

### 公共接口不变
`PipelineMaterialBatch` 的所有公共方法保持不变：
```cpp
// 这些方法的签名和行为完全相同
void Add(DrawNode *node);
void Clear();
void SetCameraInfo(const CameraInfo *ci);
void Finalize();
void UpdateTransformData();
void UpdateMaterialInstanceData(PrimitiveComponent *);
void Render(RenderCmdBuffer *);
```

### RenderBatchMap 无需修改
```cpp
// 完全兼容，无需任何修改
render_batch_map.Begin(camera_info);
render_batch_map.End();
render_batch_map.Render(cmd_buf);
render_batch_map.UpdateTransformData();
```

### RenderCollector 无需修改
```cpp
// 完全兼容，无需任何修改
render_collector.Expand(scene_node);
render_collector.Render(cmd_buf);
render_collector.UpdateTransformData();
```

---

## 文件变更总结

### 新增文件 (2个)
1. **inc/hgl/graph/PipelineMaterialRenderer.h**
   - PipelineMaterialRenderer 类声明
   - DrawBatch 结构定义（从原头文件移动）

2. **src/SceneGraph/render/PipelineMaterialRenderer.cpp**
   - PipelineMaterialRenderer 类实现
   - 渲染相关方法：Render, BindVAB, Draw, ProcIndirectRender
   - DrawBatch::Set 方法实现

### 修改文件 (3个)
1. **inc/hgl/graph/PipelineMaterialBatch.h**
   - 移除 DrawBatch 结构（移至 PipelineMaterialRenderer.h）
   - 移除渲染状态成员变量
   - 添加 renderer 成员
   - 移除渲染相关私有方法声明

2. **src/SceneGraph/render/PipelineMaterialBatch.cpp**
   - 移除 DrawBatch::Set 实现（移至 PipelineMaterialRenderer.cpp）
   - 简化 BuildBatches（使用局部变量代替成员变量）
   - 简化 Render 方法（委托给renderer）
   - 移除 BindVAB, Draw, ProcIndirectRender 实现

3. **src/SceneGraph/CMakeLists.txt**
   - 添加 PipelineMaterialRenderer.h 和 .cpp 到构建列表

---

## 测试建议

### 1. 功能测试
- ✅ 验证渲染输出与拆分前一致
- ✅ 测试不同材质和管线组合
- ✅ 测试直接绘制和间接绘制路径
- ✅ 测试变换数据更新
- ✅ 测试材质实例数据更新

### 2. 性能测试
- 测试大量节点场景（1000+）
- 测试频繁更新场景
- 对比拆分前后的性能

### 3. 集成测试
- 验证 RenderBatchMap 正常工作
- 验证 RenderCollector 正常工作
- 验证与其他渲染模块的协作

---

## 优点总结

### 设计层面
1. **职责清晰** - 收集和渲染完全分离
2. **易于理解** - 每个类的功能一目了然
3. **便于扩展** - 可以独立扩展收集或渲染逻辑
4. **便于测试** - 可以单独测试收集和渲染

### 实现层面
1. **状态隔离** - 渲染状态和收集状态完全独立
2. **耦合降低** - BuildBatches 不再依赖渲染状态
3. **接口简洁** - Render 方法参数明确，无隐藏依赖
4. **向后兼容** - 公共接口保持不变

### 维护层面
1. **代码清晰** - 每个类的代码量减少，更易阅读
2. **调试方便** - 问题定位更容易（收集问题 vs 渲染问题）
3. **优化空间** - 可以独立优化每个阶段
4. **文档友好** - 职责明确，文档更易编写

---

**修改日期**：2025-12-30  
**提交哈希**：8edc49e  
**相关Issue**：@hyzboy 的架构拆分请求
