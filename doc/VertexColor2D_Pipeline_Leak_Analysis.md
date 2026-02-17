# VertexColor2D Pipeline 泄漏原因分析

## 泄漏现象
从 run.log 中观察到：
```
[LEAK] Type=0x12 Handle=0xead9370000000008 Name=RenderPass_2_130_1000001002_3_1 <- :VKRenderPass
[LEAK] Type=0x13 Handle=0x5c5283000000003e Name=VertexColor2D <- :VKPipeline
```

两个对象都在 VulkanDevice destructor 时被检测为泄漏。

---

## 问题链条分析

### 层次 1：Pipeline 泄漏的直接原因

**VertexColor2D Pipeline 被创建于：**
```
D:\ULRE\src\Vulkan\VKRenderPass.cpp:86 in RenderPass::CreatePipeline()
```

**Pipeline 的生命周期设计：**
- Pipeline 在 `RenderPass::CreatePipeline()` 中创建
- Pipeline 被添加到 `RenderPass::pipeline_list` 容器中
- **应该在** `RenderPass::~RenderPass()` 时，通过 `pipeline_list.Clear()` 销毁所有 Pipeline

**问题：** RenderPass::~RenderPass() 的日志从未出现在 run.log 中！

这意味着：**RenderPass 对象本身没有被销毁**，所以其包含的 Pipeline 也不会被销毁。

---

### 层次 2：RenderPass 泄漏的原因

**RenderPass 被创建于：**
```
D:\ULRE\src\SceneGraph\module\RenderPassManager.cpp:313 in AcquireRenderPass()
```

**RenderPass 的生命周期设计：**
1. `AcquireRenderPass()` 创建 RenderPass 对象
2. 调用 `RenderPassList.Add(key, rp)` 添加到缓存
3. **应该在** `RenderPassManager::Release()` 时删除所有缓存的 RenderPass

**问题：从 run.log 观察**
```
[DEBUG] Calling Release() on module: RenderPassManager
[DEBUG] Release() complete for: RenderPassManager
...
[DEBUG] ~RenderPassManager() - RenderPassList.size()=0
[DEBUG] ~RenderPassManager() complete
```

**关键发现：** RenderPassList.size()=0 ← **这是异常的！**

如果 RenderPass 被成功添加到 RenderPassList，那么 Release() 时应该有类似以下的日志输出：
```
[DEBUG] RenderPassManager::Release() - Deleting RenderPass: RenderPass_2_130_1000001002_3_1
```

但日志中**完全没有这样的输出**！

---

### 层次 3：RenderPassList 为空的原因

#### 可能性 1：RenderPassList.Add() 失败
```cpp
bool Add(const K& key, const V& value) {
    auto [it, inserted] = map_data.try_emplace(key, value);
    return inserted;  // 当 key 已存在时返回 false
}
```

- 原代码没有检查 Add() 的返回值
- 如果 Add() 失败（key 重复或内存异常），RenderPass 会被创建但不会添加到列表中

**当前代码会导致的后果：**
```cpp
RenderPassList.Add(key,rp);  // 返回值被忽略
// 如果 Add() 返回 false，rp 永远不会被缓存
// 但也不会被删除！
```

#### 可能性 2：RenderPass 来自不同的来源
某些地方可能直接创建 RenderPass，而不是通过 AcquireRenderPass()。

需确认：是否存在其他 RenderPass 创建入口？

---

## 架构设计缺陷

### 当前设计存在的问题：

1. **引用计数缺失**
   ```
   SwapchainModule::sc_render_pass ── (指向) ──┐
                                              │
   RenderPassManager::RenderPassList ───────(缓存)──┤
                                              │
                          RenderPass Object ──┤
                               │
                              ├─ pipeline_list (包含 VertexColor2D Pipeline)
                              └─ ...其他资源
   ```
   
   - SwapchainModule 持有原始指针，但**不负责销毁**
   - RenderPassManager 应该销毁，但如果缓存失败...

2. **缓存添加失败未被检测**
   ```cpp
   rp = CreateRenderPass(...);  // 新建 RenderPass 对象
   RenderPassList.Add(key, rp);  // 返回值被忽略 ← BUG
   return rp;                     // 返回 RenderPass 指针
   ```

3. **多个模块引用问题**
   - SwapchainModule 在 Release() 中没有清理 `sc_render_pass`
   - 虽然意图是 RenderPassManager 来管理，但如果添加失败...

---

## 根本原因总结

### 最可能的原因链：

1. **AcquireRenderPass() 被调用**
   - 创建新 RenderPass 对象 ✓
   - 调用 `RenderPassList.Add(key, rp)` → **失败？或未被检查？**

2. **结果：RenderPass 对象悬空**
   - 对象被创建并分配内存 ✓
   - 对象被返回给 SwapchainModule（sc_render_pass）✓
   - 对象**未被添加到 RenderPassList** ✗
   
3. **Cleanup 时 RenderPassList 为空**
   - RenderPassManager::Release() 找不到要删除的对象
   - RenderPass 对象永远不被销毁
   - Pipeline 对象随之泄漏

4. **SwapchainModule 的问题**
   - 清楚持有 sc_render_pass 指针但从未销毁
   - 依赖 RenderPassManager 来管理，但如果对象未被缓存...

---

## 修复方案

### 立即修复（已实施）：

1. **添加日志追踪 Add() 返回值**
   ```cpp
   bool add_success = RenderPassList.Add(key, rp);
   std::cout << "[RenderPassManager::AcquireRenderPass] After Add(), add_success=" 
             << add_success << " RenderPassList.size()=" << RenderPassList.GetCount() << std::endl;
   ```

2. **增强 Release() 日志**
   ```cpp
   void Release() override {
       std::cout << "[DEBUG] RenderPassManager::Release() - RenderPassList.GetCount()=" 
                 << RenderPassList.GetCount() << std::endl;
       // ... 删除过程 ...
   }
   ```

3. **SwapchainModule Release() 日志**
   - 跟踪 sc_render_pass 的生命周期

### 长期修复建议：

#### 方案 A：Reference Counting（推荐）
```cpp
class RenderPass {
    std::shared_ptr<RenderPassData> data;  // 自动管理生命周期
};
```

#### 方案 B：显式所有权移交
```cpp
// SwapchainModule::Release() 中
if (sc_render_pass) {
    render_pass_manager->RemoveRenderPass(key);  // 显式告诉管理器删除
    sc_render_pass = nullptr;
}
```

#### 方案 C：统一的生命周期管理
```cpp
// RenderPassManager 跟踪所有引用
map<RenderPass*, set<void*>> reference_holders;
// 所有持有 RenderPass 的模块都注册
void RegisterHolder(RenderPass* rp, void* holder);
void UnregisterHolder(RenderPass* rp, void* holder);
// 只有当所有引用都被移除后才删除
```

---

## 验证步骤

### 1. 编译新版本
```bash
cmake --build build --config Release
```

### 2. 运行示例
```bash
.\build\Release\draw_triangle.exe 2>&1 | tee new_run.log
```

### 3. 分析新日志输出
查找：
- `[RenderPassManager::AcquireRenderPass] After Add(), add_success=`
- `[RenderPassManager::Release() - RenderPassList.GetCount()=`
- 比较添加和删除时的列表大小

---

## 预期结果

如果修复成功，日志应该显示：
```
[RenderPassManager::AcquireRenderPass] Created new RenderPass 'RenderPass_2_130_1000001002_3_1'
[RenderPassManager::AcquireRenderPass] Before adding to list, RenderPassList.size()=0
[RenderPassManager::AcquireRenderPass] After Add(), add_success=true RenderPassList.size()=1

... 程序运行 ...

[DEBUG] RenderPassManager::Release() - RenderPassList.GetCount()=1
[DEBUG] RenderPassManager::Release() - Deleting RenderPass: RenderPass_2_130_1000001002_3_1
[RenderPass::~RenderPass] Destroying RenderPass with 1 pipelines
[RenderPass::~RenderPass] Clearing Pipeline [0]: 'VertexColor2D'
Pipeline Destructor: DESTROYED (handle=...)
```

如果 `add_success=false`，说明问题是 Add() 失败，需要进一步调查 UnorderedMap 的状态。

---

## 参考信息

**相关文件：**
- [RenderPassManager.h](inc/hgl/graph/module/RenderPassManager.h) - 缓存管理逻辑
- [RenderPassManager.cpp](src/SceneGraph/module/RenderPassManager.cpp) - 缓存实现
- [VKRenderPass.h/cpp](src/Vulkan/VKRenderPass.cpp) - Pipeline 生命周期管理
- [SwapchainModule.cpp](src/SceneGraph/module/SwapchainModule.cpp) - sc_render_pass 引用

**日志位置：**
- run.log - 原始泄漏日志
- new_run.log - 添加详细日志后的输出

