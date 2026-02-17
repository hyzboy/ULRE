# ObjectTracker 本地追踪系统实现指南

## 📋 架构总览

```
主程序运行时                             崩溃/泄露时
├─ ObjectIdGenerator                     ├─ dump_to_file()
│  (原子自增ID)                          └─ allocation_trace.bin
├─                                       
├─ thread_local allocation_stack         离线分析
├─ (std::source_location栈)              ├─ analyze_trace.py query <id>
│                                        ├─ analyze_trace.py stats
├─ AllocationTracker                     └─ analyze_trace.py list
│  (环形缓冲区)
│  CAPACITY = 1,000,000 events
│  Memory = ~336 MB
│
└─ ScopeCapture RAII
   (自动push/pop栈)
```

---

## 🚀 快速开始

### 1. 初始化（main函数）

```cpp
#include <hgl/utils/ObjectTracker.h>

int main() {
    using namespace hgl::utils;
    
    // 初始化全局追踪器
    initialize_object_tracker();
    
    // 注册崩溃处理
    setup_crash_handler();
    
    // ... 你的程序逻辑
    
    // 正常退出时清理
    shutdown_object_tracker();
    return 0;
}

void setup_crash_handler() {
    std::signal(SIGSEGV, [](int) {
        if (hgl::utils::g_object_tracker) {
            hgl::utils::g_object_tracker->dump_to_file("allocation_trace.bin");
        }
        std::exit(1);
    });
}
```

---

### 2. 在关键函数添加栈捕获

```cpp
// 在对象分配的入口点添加 ScopeCapture
void User::CreateObject() {
    HGL_CAPTURE_SCOPE();  // 等同于: hgl::utils::ScopeCapture scope;
    
    // 创建渲染系统资源
    uint64_t buffer_id = renderSystem->AllocateBuffer();
    // ...
}

void RenderSystem::AllocateBuffer() {
    HGL_CAPTURE_SCOPE();  // 自动记录这一层
    
    uint64_t id = HGL_TRACK_ALLOCATION("Geometry Buffer", 
                                        hgl::graph::ObjectTypeTag::VertexBuffer);
    
    device->CreateBuffer(id, ...);
    return id;
}

void VulkanDevice::CreateBuffer(uint64_t id, ...) {
    HGL_CAPTURE_SCOPE();  // 自动记录这一层
    
    // ...
    return buffer_ptr;
}
```

---

### 3. 使用结果

**分配时**（主程序）：
```
追踪记录:
  object_id:     12345
  object_type:   VertexBuffer
  object_name:   "Geometry Buffer"
  stack_depth:   3
  timestamp:     1234567890.123s
```

**泄露时**（输出）：
```
[LEAK] buffer_id=12345
```

**查询时**（离线分析）：
```bash
$ python3 analyze_trace.py allocation_trace.bin query 12345

object_id:     12345
object_type:   VertexBuffer
object_name:   "Geometry Buffer"
timestamp:     1234567890.123s
stack_depth:   3

Allocation Stack:
  [0] file_hash=0x7f8a01234567 line=456 col=20 func_hash=0x1a2b3c4d
  [1] file_hash=0x7f8a01234567 line=123 col=10 func_hash=0x5e6f7g8h
  [2] file_hash=0x7f8a01234567 line=789 col=15 func_hash=0x9i0j1k2l
```

---

## 🔧 集成到RenderPrimitiveBatchSystem

### 修改 `src/ecs/systems/render/RenderPrimitiveBatchSystem.cpp`

```cpp
#include <hgl/utils/ObjectTracker.h>

void ReallocICB(graph::VulkanDevice* device,
                const std::vector<RenderItem*>& list,
                graph::IndirectDrawBuffer*& icb_draw_out,
                graph::IndirectDrawIndexedBuffer*& icb_draw_indexed_out,
                const ECSContext* context = nullptr)
{
    HGL_CAPTURE_SCOPE();  // ← 添加这行
    
    // ... 现有逻辑
    
    // 记录分配
    uint64_t draw_id = HGL_TRACK_ALLOCATION(
        "IndirectDrawBuffer",
        hgl::graph::ObjectTypeTag::IndirectDrawBuffer
    );
    
    icb_draw_out = device->CreateIndirectDrawBuffer(icb_new_count, draw_id);
    
    uint64_t indexed_id = HGL_TRACK_ALLOCATION(
        "IndirectDrawIndexedBuffer",
        hgl::graph::ObjectTypeTag::IndirectDrawIndexedBuffer
    );
    
    icb_draw_indexed_out = device->CreateIndirectDrawIndexedBuffer(icb_new_count, indexed_id);
}
```

---

## 📊 分析工具使用

### 查询单个对象

```bash
$ python3 analyze_trace.py allocation_trace.bin query 12345

object_id:     12345
object_type:   VertexBuffer
object_name:   "Geometry Buffer"
timestamp:     1234567890.123s
stack_depth:   3

Allocation Stack:
  [0] file_hash=0x... line=456 col=20 func_hash=0x...
  [1] file_hash=0x... line=123 col=10 func_hash=0x...
  [2] file_hash=0x... line=789 col=15 func_hash=0x...
```

### 统计信息

```bash
$ python3 analyze_trace.py allocation_trace.bin stats

Total Events: 523456

By Type:
  IndirectDrawBuffer            123456
  IndirectDrawIndexedBuffer      98765
  VertexBuffer                   87654
  ...

By Name (Top 20):
  IndirectDrawBuffer                     123456
  IndirectDrawIndexedBuffer               98765
  Geometry Buffer                         45678
  ...
```

### 列出前N个事件

```bash
$ python3 analyze_trace.py allocation_trace.bin list 50

Listing first 50 events:

[0] ID=        1 Type=RenderCmdBuf         Name=Frame0              Depth= 2 T=0.001s
[1] ID=        2 Type=Queue                Name=MainQueue           Depth= 1 T=0.002s
[2] ID=        3 Type=VertexBuffer         Name=Geometry Buffer     Depth= 3 T=0.003s
...
```

---

## 🎯 关键特性

### 1. 极低开销
- 分配时开销: ~110 ns（原子操作 + 栈操作）
- 存储开销: 4B per object（只存object_id）
- 追踪缓冲区: 336 MB（100万事件）

### 2. 线程安全
- 无锁ID生成（原子自增）
- thread_local栈（每线程独立）
- Ring buffer保护（互斥锁）

### 3. 崩溃恢复
- 导出时不依赖malloc
- 二进制格式（紧凑）
- 离线分析（不需要符号服务）

### 4. 层级追踪
```
User::CreateObject()
  ↓ ScopeCapture
RenderSystem::AllocateBuffer()
  ↓ ScopeCapture
VulkanDevice::CreateBuffer()
  ↓ HGL_TRACK_ALLOCATION
[记录: User → RenderSystem → Device 的完整调用链]
```

---

## 📈 扩展建议

### Phase 2（将来）
1. **地址符号化**: 用 addr2line/dbghelp 将指针转换为 file:line:function
2. **热点分析**: 统计何种分配最多
3. **泄露检测**: 对比两次dump找新增object
4. **时序分析**: 按timestamp排序

### Phase 3（可选）
1. **统计API**: 提供栈中同类object聚类统计
2. **实时监控**: 不导出文件，实时流式查询
3. **自动化报告**: 生成markdown/html文档

---

## ⚙️ 配置选项

### 调整环形缓冲区大小

```cpp
// ObjectTracker.h - 修改CAPACITY
template<size_t CAPACITY = 100000>  // 从1000000改为100000
class AllocationTracker { ... };
```

### 条件编译控制

```cpp
#ifdef HGL_ENABLE_OBJECT_TRACKING
    initialize_object_tracker();
#endif
```

---

## 🐛 常见问题

**Q: 内存占用太大？**
A: 改 CAPACITY 为 100K（只记录最近100K个分配），占用~33MB

**Q: 想要符号化地址？**
A: 编译时添加 `-g -fno-omit-frame-pointer`，分析时用 `addr2line file_path 0xaddress`

**Q: 多进程支持？**
A: 每进程独立dump不同文件，分析工具合并后处理

**Q: 如何禁用追踪？**
A: 不调用 `initialize_object_tracker()`，或改成 no-op 宏

