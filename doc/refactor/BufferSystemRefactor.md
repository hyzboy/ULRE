# Buffer System 完全重构计划

> 文档版本：1.0  
> 创建日期：2026-02-22  
> 适用项目：ULRE  

---

## 目录

1. [背景与动机](#1-背景与动机)
2. [现有架构问题分析](#2-现有架构问题分析)
3. [目标架构](#3-目标架构)
4. [重构计划](#4-重构计划)
   - [Phase 1 — DeviceBuffer 清理](#phase-1--devicebuffer-清理)
   - [Phase 2 — 多缓冲管理层清理](#phase-2--多缓冲管理层清理)
   - [Phase 3 — 访问器层清理](#phase-3--访问器层清理)
   - [Phase 4 — ECS System 层清理](#phase-4--ecs-system-层清理)
   - [Phase 5 — Policy 系统清理](#phase-5--policy-系统清理)
5. [执行顺序与依赖关系](#5-执行顺序与依赖关系)
6. [验收标准](#6-验收标准)
7. [文件变更清单](#7-文件变更清单)

---

## 1. 背景与动机

### 1.1 现状

ULRE 的 Buffer 系统历经多次迭代，积累了多层封装：

- `DeviceBuffer` —— VK 原始封装
- `StagedBuffer / ReBarBuffer / RingBuffer` —— 多缓冲管理
- `BufferAccessBase / StructuredBufferAccessor` —— 类型化访问器
- `BufferTransferAgent / BufferWriteAgent` —— 写入代理中间层
- `BufferCommitQueue / BufferUpdateQueue` —— 提交队列
- `BufferPolicy / BufferPolicyImpl` —— 运行时策略系统
- `RenderBufferUploadSystem / RenderBufferCommitSystem` —— ECS 驱动层

### 1.2 动机

当前实现存在以下核心问题：

1. **职责泄漏**：ECS System 的调度职责泄漏进了缓冲对象内部（`auto_commit`、`CommitQueue` 指针等）
2. **层次混乱**：多个中间层（`TransferAgent`、`WriteAgent`、`UpdateQueue`）重复抽象，增加调试难度
3. **过度设计**：`BufferCommitQueue` 的预算/优先级/deadline 系统在实际场景中几乎未被使用
4. **接口冗余**：`BufferWriteAgent` 和 `BufferTransferAgent` 的接口与 Layer2 缓冲对象本身高度重叠
5. **队列重复**：`BufferUpdateQueue` 的 `AddUpdate/FlushAll` 逻辑与 `BufferCommitQueue` 高度重叠，两套队列并存

### 1.3 目标

> 开发者只需拿到缓冲对象，写入数据，剩下的交给框架。

---

## 2. 现有架构问题分析

### 2.1 提交路径过长

```
Write()
  → TransferAgent
    → StagedBuffer
      → UpdateQueue
        → CommitQueue
          → ECS System
```

中间 5~6 层导致：
- 调试困难，无法快速定位数据在哪一层丢失
- 每层都有独立的 `dirty` 标记，容易出现状态不一致
- 新接入开发者学习成本极高

### 2.2 ECS 职责泄漏进缓冲对象

```cpp
// 这些字段不应该出现在 BufferAccessBase 中
bool               auto_commit;       // ECS 的职责
BufferCommitQueue* commit_queue;      // ECS 的职责
void               RegisterAutoCommit();   // ECS 的职责
void               UnregisterAutoCommit(); // ECS 的职责
```

缓冲对象不应该知道"何时提交"、"如何调度"，这是 ECS System 的工作。

### 2.3 两套队列并存

`BufferUpdateQueue` 负责收集 `StagedBuffer` 的脏区间，`BufferCommitQueue` 又在其上层再做一次调度。
两者职责重叠，`RenderBufferUploadSystem` 和 `RenderBufferCommitSystem` 双 System 并存，
实际上一个 System 遍历 `IGPUBuffer` 列表即可完成全部工作。

### 2.4 Policy 系统过于复杂

`BufferPolicy` 结构体包含 20+ 字段，三套枚举（`BufferCommitPolicy`、`BufferUpdateClass`、`BufferPriority`）职责部分重叠，绝大多数字段在运行时从未被查询。

### 2.5 接口冗余

`StagedBufferTransferAgent` 同时存在两个构造函数，分别对应新旧两套路径，说明中间层本身的存在价值已经动摇。

---

## 3. 目标架构

### 3.1 分层结构

```
┌─────────────────────────────────────────────┐
│  Layer 3：开发者 API 层                      │
│                                             │
│  StructuredBufferAccessor<T>                │
│  BufferAccessor<T>                          │
│                                             │
│  职责：类型安全的读写接口                    │
│  规则：只持有 IGPUBuffer* 引用，不含提交逻辑 │
└─────────────────┬───────────────────────────┘
                  │ 持有引用
┌─────────────────▼───────────────────────────┐
│  Layer 2：多缓冲管理层                       │
│                                             │
│  StagedBuffer   ReBarBuffer   RingBuffer    │
│                                             │
│  职责：管理内部多个 DeviceBuffer 的协调      │
│        暴露统一的 Write/Map/MarkDirty 接口   │
│  规则：不含调度逻辑，不持有队列引用          │
└─────────────────┬───────────────────────────┘
                  │ 持有
┌─────────────────▼───────────────────────────┐
│  Layer 1：VK 原始封装层                      │
│                                             │
│  DeviceBuffer                               │
│                                             │
│  职责：VkBuffer + DeviceMemory 的纯容器      │
│  规则：不含任何业务逻辑                      │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│  Layer 0：ECS 驱动层（独立，不侵入上方三层） │
│                                             │
│  RenderBufferUploadSystem                   │
│                                             │
│  职责：每帧遍历脏缓冲，执行 CopyToDevice    │
│        插入 pipeline barrier               │
└─────────────────────────────────────────────┘
```

### 3.2 核心原则

| 原则 | 说明 |
|------|------|
| 单向依赖 | 上层依赖下层，下层不知道上层存在 |
| 职责单一 | 每层只做一件事 |
| ECS 隔离 | 调度/提交逻辑完全在 ECS System 中，不泄漏进缓冲对象 |
| 简单接口 | Layer2 统一实现 `IGPUBuffer` 接口，开发者无需了解内部实现 |
| 无中间队列 | `UpdateQueue` / `CommitQueue` 全部删除，ECS System 直接遍历注册表 |

### 3.3 Layer2 统一接口

```cpp
class IGPUBuffer {
public:
    virtual ~IGPUBuffer() = default;

    // 写入数据（开发者调用）
    virtual bool   Write(const void* data, VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual void*  Map(VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual void   Unmap() = 0;

    // 脏标记（只记录，不触发任何提交）
    virtual void   MarkDirty(VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual bool   IsDirty() const = 0;
    virtual void   ClearDirty() = 0;

    // ECS System 调用：执行实际的 GPU 数据复制
    virtual void   CopyToDevice(VkCommandBuffer cmd) = 0;

    // 供 Descriptor 绑定使用
    virtual DeviceBuffer* GetDeviceBuffer() = 0;
};
```

---

## 4. 重构计划

---

### Phase 1 — DeviceBuffer 清理

**目标：** `DeviceBuffer` 成为 `VkBuffer + DeviceMemory` 的纯容器，不含任何业务逻辑。

#### Step 1.1 确认保留内容

```cpp
class DeviceBuffer {
    VkDevice            device;
    VkBuffer            buffer;
    DeviceMemory*       memory;
    VkDeviceSize        size;
    VkBufferUsageFlags  usage;

public:
    VkBuffer                GetBuffer()         const;
    VkDeviceSize            GetSize()           const;
    VkDescriptorBufferInfo  GetDescriptorInfo() const;
};
```

#### Step 1.2 从 DeviceBuffer 删除的内容

| 删除项 | 原因 |
|--------|------|
| `owner_device` 指针 | 生命周期由 ECS/创建者管理 |
| `auto_commit_proxy` | 不属于 VK 封装 |
| `transfer_agent` | Layer2 的职责 |
| `UntrackBuffer()` 调用 | ECS 层管理生命周期 |
| `GetOwnerDevice()` | 向上依赖，破坏分层 |
| `GetBufferCommitQueue()` | 同上 |

#### Step 1.3 VulkanDevice 工厂方法简化

**删除：**
```cpp
// 删除带 Policy 参数的重载
DeviceBuffer* CreateBuffer(..., BufferUpdateClass, BufferAllocPolicy);
```

**保留/新增：**
```cpp
// 只保留最小化工厂接口
DeviceBuffer* CreateBuffer(
    VkBufferUsageFlags usage,
    VkDeviceSize       size,
    MemoryUsage        memUsage
);

DeviceBuffer* CreateBufferWithData(
    VkBufferUsageFlags usage,
    VkDeviceSize       size,
    const void*        data
);
```

#### Step 1.4 验收标准

- `VKBuffer.h` 不再 `#include` 以下任何头文件：
  - `BufferPolicy.h`
  - `VKBufferTransferAgent.h`
  - `VKBufferCommitQueue.h`
  - `VKBufferUpdateQueue.h`
- `DeviceBuffer` 析构函数中无 `UntrackBuffer`、`delete transfer_agent`、`delete auto_commit_proxy` 调用

---

### Phase 2 — 多缓冲管理层清理

**目标：** 每个 Layer2 类型只负责内部多个 `DeviceBuffer` 的协调，实现 `IGPUBuffer` 接口，不含调度逻辑。

#### Step 2.1 新增 IGPUBuffer 接口文件

```
新增文件：inc/hgl/vk/IGPUBuffer.h
```

```cpp
// filepath: e:\ULRE\inc\hgl\vk\IGPUBuffer.h
#pragma once
#include <hgl/vk/VK.h>

namespace hgl::graph {

class DeviceBuffer;

class IGPUBuffer {
public:
    virtual ~IGPUBuffer() = default;

    virtual bool          Write(const void* data, VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual void*         Map(VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual void          Unmap() = 0;

    virtual void          MarkDirty(VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual bool          IsDirty()  const = 0;
    virtual void          ClearDirty() = 0;

    virtual void          CopyToDevice(VkCommandBuffer cmd) = 0;
    virtual DeviceBuffer* GetDeviceBuffer() = 0;
};

} // namespace hgl::graph
```

#### Step 2.2 重构 StagedBuffer

**删除：**
```cpp
BufferUpdateQueue* update_queue;  // 删除队列引用
// 删除构造函数中的 queue 参数
```

**保留/修改：**
```cpp
class StagedBuffer : public IGPUBuffer {
    DeviceBuffer*   staging_buffer;   // CPU visible
    DeviceBuffer*   device_buffer;    // GPU only
    DirtyRangeList  dirty_ranges;     // 只记录脏区间，不触发任何操作

public:
    // MarkDirty 只记录，不再调用 update_queue->AddUpdate()
    void MarkDirty(VkDeviceSize offset, VkDeviceSize size) override;

    // ECS System 调用
    void CopyToDevice(VkCommandBuffer cmd) override;
};
```

#### Step 2.3 重构 RingBuffer

**删除：**
```cpp
// 删除 DeviceBufferRingWriter 包装层（内联逻辑）
// 删除 BufferWriteAgent 继承
```

**保留/修改：**
```cpp
class RingBuffer : public IGPUBuffer {
    DeviceBuffer*  buffers[HGL_RING_FRAMES];
    uint32_t       current_frame;
    VkDeviceSize   element_size;

public:
    // 由 ECS System 在帧开始时调用
    void AdvanceFrame(uint32_t frame_index);
};
```

#### Step 2.4 删除文件清单

| 文件 | 原因 |
|------|------|
| `VKBufferTransferAgent.h/cpp` | Layer2 不需要 Agent 中间层 |
| `VKBufferWriteAgent.h` | 职责被 IGPUBuffer 取代 |
| `VKBufferUpdateQueue.h/cpp` | 队列职责移交 ECS System + BufferRegistry |
| `VKRingBufferWrapper.h` | RingBuffer 直接实现 IGPUBuffer |
| `DeviceBufferRingWriter.h` | 内联进 RingBuffer |
| `BufferPolicyImpl.h/cpp` | Policy 决策移入工厂函数 |
| `VKBufferCommitQueue.h/cpp` | 整个预算系统删除 |

#### Step 2.5 验收标准

- Layer2 头文件只 `#include` Layer1 头文件
- `StagedBuffer` 构造函数不需要 `queue` 参数，`MarkDirty` 不再调用任何队列方法
- `BufferUpdateQueue` 和 `BufferCommitQueue` 相关代码完全从项目中消失

---

### Phase 3 — 访问器层清理

**目标：** 访问器只持有 `IGPUBuffer*` 引用，提供类型安全读写，不含提交逻辑。

#### Step 3.1 重构 BufferAccessBase

**删除：**
```cpp
bool               auto_commit;
BufferCommitQueue* commit_queue;
bool               owns_buffer;         // 所有权改由创建者管理
void               RegisterAutoCommit();
void               UnregisterAutoCommit();
```

**简化为：**
```cpp
class BufferAccessBase {
protected:
    IGPUBuffer* gpu_buffer = nullptr;

public:
    void        SetBuffer(IGPUBuffer* buf);
    IGPUBuffer* GetBuffer() const { return gpu_buffer; }
    bool        IsDirty()   const;

    bool        Write(const void* data, VkDeviceSize offset, VkDeviceSize size);
};
```

#### Step 3.2 重构 StructuredBufferAccessor\<T\>

```cpp
template<typename T>
class StructuredBufferAccessor : public BufferAccessBase {
public:
    // 写入单个元素
    bool Write(const T& value, uint32_t index = 0) {
        return BufferAccessBase::Write(
            &value,
            index * sizeof(T),
            sizeof(T)
        );
    }

    // 写入数组
    bool WriteArray(const T* values, uint32_t count, uint32_t offset = 0) {
        return BufferAccessBase::Write(
            values,
            offset * sizeof(T),
            count  * sizeof(T)
        );
    }

    // 无任何 Commit / Flush / AutoCommit 相关方法
};
```

#### Step 3.3 删除内容清单

| 删除项 | 位置 | 原因 |
|--------|------|------|
| `auto_commit` 字段 | `BufferAccessBase` | ECS 职责 |
| `SetAutoCommit()` | `BufferAccessBase` | ECS 职责 |
| `commit_queue` 字段 | `BufferAccessBase` | ECS 职责 |
| `RegisterAutoCommit()` | `BufferAccessBase` | ECS 职责 |
| `UnregisterAutoCommit()` | `BufferAccessBase` | ECS 职责 |
| `owns_buffer` 字段 | `BufferAccessBase` | 由创建者管理 |
| 队列注册逻辑 | `SetBuffer()` 内部 | ECS 职责 |

#### Step 3.4 验收标准

- `VKBufferAccessBase.h` 不再 `#include` `VKBufferCommitQueue.h` 或 `VKBufferUpdateQueue.h`
- `StructuredBufferAccessor` 无任何 `Commit`/`Flush`/`AutoCommit` 相关公开方法
- `BufferAccessBase::SetBuffer()` 函数体少于 10 行

---

### Phase 4 — ECS System 层清理

**目标：** ECS System 统一持有所有 `IGPUBuffer*`，负责每帧 flush/submit，不向下层泄漏任何调度逻辑。

#### Step 4.1 新增 BufferRegistry

```
新增文件：inc/hgl/ecs/BufferRegistry.h
```

```cpp
// filepath: e:\ULRE\inc\hgl\ecs\BufferRegistry.h
#pragma once
#include <hgl/vk/IGPUBuffer.h>
#include <vector>

namespace hgl::ecs {

/**
 * 全局缓冲注册表
 * Layer2 对象创建后注册到此处
 * ECS System 遍历此表驱动每帧上传
 * 替代原有的 BufferUpdateQueue + BufferCommitQueue 两套队列
 */
class BufferRegistry {
    std::vector<hgl::graph::IGPUBuffer*> buffers;

public:
    void Register  (hgl::graph::IGPUBuffer* buf);
    void Unregister(hgl::graph::IGPUBuffer* buf);

    const std::vector<hgl::graph::IGPUBuffer*>& GetAll() const {
        return buffers;
    }
};

} // namespace hgl::ecs
```

#### Step 4.2 重构 RenderBufferUploadSystem

```cpp
void RenderBufferUploadSystem::Update(float deltaTime) {
    auto cmd = device->GetCurrentTransferCommandBuffer();

    // 遍历所有注册缓冲，只处理脏的
    for (auto* buf : registry->GetAll()) {
        if (buf->IsDirty()) {
            buf->CopyToDevice(cmd);
            buf->ClearDirty();
        }
    }

    // 插入 pipeline barrier，确保后续 render pass 可见
    device->InsertBufferMemoryBarrier(cmd);
}
```

#### Step 4.3 合并/删除 RenderBufferCommitSystem

`BufferCommitQueue` 整个预算系统删除后，`RenderBufferCommitSystem` 的职责全部并入 `RenderBufferUploadSystem`。

```
删除文件：inc/hgl/ecs/systems/render/RenderBufferCommitSystem.h
删除文件：src/ecs/systems/render/RenderBufferCommitSystem.cpp
```

#### Step 4.4 验收标准

- `RenderBufferUploadSystem` 不再 `#include` 任何 Layer2/Layer3 具体实现头文件，只依赖 `IGPUBuffer.h`
- `RenderBufferCommitSystem` 文件从项目中删除，编译通过
- `BufferRegistry` 是唯一的 Buffer 生命周期跟踪点，无任何运行时队列

---

### Phase 5 — Policy 系统清理

**目标：** `BufferPolicy` 只作为创建时参数，不在运行时影响任何行为。

#### Step 5.1 简化 BufferPolicy

**删除以下枚举值/字段：**

| 删除项 | 原因 |
|--------|------|
| `BufferCommitPolicy` 整个枚举 | 由 IGPUBuffer 实现类型决定 |
| `BufferUpdateClass` 整个枚举 | 由工厂函数决定 |
| `BufferPriority` | 预算系统删除后无意义 |
| `BufferSubmitTiming` | 同上 |
| `BufferDropPolicy` | 同上 |
| `deadline_frames` | 同上 |
| `budget_group` | 同上 |

**保留：**
```cpp
// 只保留创建时需要的硬件路径选择
enum class BufferMemoryType {
    GpuOnly,        // 纯 GPU，使用 StagedBuffer 路径
    CpuVisible,     // ReBAR 或 UMA，直接写入
    RingCpuVisible, // Ring 模式
};
```

#### Step 5.2 用工厂函数替代 PolicyImpl

**删除：** `BufferPolicyImpl.h/cpp` 中所有运行时 Policy 决策逻辑

**替换为简单工厂函数：**

```cpp
// filepath: e:\ULRE\inc\hgl\vk\BufferFactory.h
#pragma once
#include <hgl/vk/IGPUBuffer.h>

namespace hgl::graph::BufferFactory {

    IGPUBuffer* CreateCameraUBO      (VulkanDevice*, VkDeviceSize size);
    IGPUBuffer* CreateStaticMeshVAB  (VulkanDevice*, VkDeviceSize size);
    IGPUBuffer* CreateDynamicMeshVAB (VulkanDevice*, VkDeviceSize size);
    IGPUBuffer* CreateRingBuffer     (VulkanDevice*, VkDeviceSize size, uint32_t frames);

} // namespace hgl::graph::BufferFactory
```

工厂函数内部根据物理设备属性（ReBAR/UMA/Discrete）选择实际的 Layer2 实现类型，对上层完全透明。

#### Step 5.3 验收标准

- `BufferPolicy.h` 枚举总数不超过 1 个（`BufferMemoryType`）
- `BufferPolicyImpl.cpp` 从项目中删除
- 所有 `BufferUpdateClass::MeshStatic` 等引用替换为对应工厂函数调用

---

## 5. 执行顺序与依赖关系

```
Phase 1 ── DeviceBuffer 清理
    │
    ▼
Phase 2 ── Layer2 多缓冲管理层清理 ◄── Phase 5 (可并行)
    │
    ▼
Phase 3 ── 访问器层清理
    │
    ▼
Phase 4 ── ECS System 层清理
```

> **建议：** Phase 1 和 Phase 5 可以同时开始，互不依赖。  
> Phase 2 必须在 Phase 1 完成后开始。  
> Phase 3、4 必须在 Phase 2 完成后开始。

---

## 6. 验收标准

### 6.1 各 Phase 验收

| Phase | 验收标准 |
|-------|---------|
| Phase 1 | `VKBuffer.h` 不再依赖 `BufferPolicy.h`、`VKBufferTransferAgent.h`、`VKBufferCommitQueue.h`、`VKBufferUpdateQueue.h` |
| Phase 2 | Layer2 所有头文件无 ECS 依赖；`StagedBuffer` 构造不需要 `queue` 参数；`MarkDirty` 不调用任何队列 |
| Phase 3 | `VKBufferAccessBase.h` 只依赖 `IGPUBuffer.h`；无 `AutoCommit` 公开方法 |
| Phase 4 | `VKBufferCommitQueue` 和 `VKBufferUpdateQueue` 从项目中完全删除；编译通过 |
| Phase 5 | `BufferPolicy.h` 枚举减少到 1 个；`BufferPolicyImpl` 文件删除 |

### 6.2 最终整体验收

| 检查项 | 标准 |
|--------|------|
| 编译 | 全项目零错误、零警告（与重构前持平） |
| 功能 | 所有现有渲染测试通过，效果无回退 |
| 依赖 | 使用工具检查头文件依赖图，Layer1 不依赖 Layer2/3 |
| 代码量 | Buffer 相关代码总行数减少 ≥ 30% |
| 接口 | 开发者写入数据只需调用 `Write()` 或 `Map()`，无需关心提交逻辑 |

---

## 7. 文件变更清单

### 7.1 新增文件

| 文件路径 | 说明 |
|---------|------|
| `inc/hgl/vk/IGPUBuffer.h` | Layer2 统一接口 |
| `inc/hgl/ecs/BufferRegistry.h` | 缓冲注册表（替代 UpdateQueue + CommitQueue） |
| `src/ecs/BufferRegistry.cpp` | 缓冲注册表实现 |
| `inc/hgl/vk/BufferFactory.h` | 工厂函数声明 |
| `src/Vulkan/BufferFactory.cpp` | 工厂函数实现 |

### 7.2 修改文件

| 文件路径 | 修改内容 |
|---------|---------|
| `inc/hgl/vk/VKBuffer.h` | 删除 Policy/Agent/CommitQueue/UpdateQueue 依赖 |
| `src/Vulkan/VKBuffer.cpp` | 删除析构中的 Agent/Proxy 清理 |
| `inc/hgl/vk/VKStagedBuffer.h` | 实现 IGPUBuffer；删除 queue 参数；MarkDirty 不再调用队列 |
| `src/Vulkan/VKStagedBuffer.cpp` | 同上 |
| `inc/hgl/vk/VKBufferAccessBase.h` | 删除 AutoCommit/CommitQueue/UpdateQueue 相关 |
| `src/Vulkan/VKBufferAccessBase.cpp` | 同上 |
| `inc/hgl/ecs/systems/render/RenderBufferUploadSystem.h` | 只依赖 IGPUBuffer + BufferRegistry |
| `src/ecs/systems/render/RenderBufferUploadSystem.cpp` | 重写 Update 逻辑 |
| `inc/hgl/vk/BufferPolicy.h` | 简化为仅 BufferMemoryType |

### 7.3 删除文件

| 文件路径 | 原因 |
|---------|------|
| `inc/hgl/vk/VKBufferTransferAgent.h` | 中间层删除 |
| `src/Vulkan/VKBufferTransferAgent.cpp` | 同上 |
| `inc/hgl/vk/VKBufferWriteAgent.h` | 被 IGPUBuffer 取代 |
| `inc/hgl/vk/VKBufferUpdateQueue.h` | 队列职责移交 BufferRegistry；ECS System 直接驱动 |
| `src/Vulkan/VKBufferUpdateQueue.cpp` | 同上 |
| `inc/hgl/vk/VKBufferCommitQueue.h` | 预算系统整体删除 |
| `src/Vulkan/VKBufferCommitQueue.cpp` | 同上 |
| `inc/hgl/vk/VKRingBufferWrapper.h` | RingBuffer 直接实现 IGPUBuffer |
| `inc/hgl/vk/DeviceBufferRingWriter.h` | 内联进 RingBuffer |
| `inc/hgl/vk/BufferPolicyImpl.h` | 替换为工厂函数 |
| `src/Vulkan/BufferPolicyImpl.cpp` | 同上 |
| `inc/hgl/ecs/systems/render/RenderBufferCommitSystem.h` | 并入 UploadSystem |
| `src/ecs/systems/render/RenderBufferCommitSystem.cpp` | 同上 |

---

*文档结束*
