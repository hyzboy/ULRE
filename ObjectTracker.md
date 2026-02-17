下面给你一份**工程级、可落地的完整架构设计**。目标：

* 运行时常驻开销 ≈ 4 bytes / object
* 无业务线程 IO
* 支持多线程
* 支持 stack 去重
* 崩溃后可离线分析
* 不依赖 malloc（避免递归污染）

我会按 **模块结构图 + 数据流 + 关键数据结构 + 文件格式 + 崩溃恢复策略** 给出。

---

# 一、总体架构图

```
                ┌──────────────────────────┐
                │      业务线程 (N)        │
                │--------------------------│
                │ 1. 分配 object_id        │
                │ 2. capture stack         │
                │ 3. stack hash            │
                │ 4. push TraceEvent       │
                └─────────────┬────────────┘
                              │
                              ▼
                ┌──────────────────────────┐
                │   Lock-Free RingBuffer   │
                └─────────────┬────────────┘
                              │
                              ▼
                ┌──────────────────────────┐
                │    后台 TraceWriter 线程 │
                │--------------------------│
                │ 1. stack 去重            │
                │ 2. 分配 stack_id         │
                │ 3. 批量写 mmap 文件      │
                └─────────────┬────────────┘
                              │
                              ▼
                ┌──────────────────────────┐
                │        trace.log         │
                │  [stack table]          │
                │  [object table]         │
                └──────────────────────────┘
```

---

# 二、运行时对象模型

## 1️⃣ 对象只保存：

```cpp
struct TrackedObject {
    uint32_t object_id;
};
```

常驻成本：**4 bytes**

---

# 三、核心运行时模块

---

## 1️⃣ ID 分配器

```cpp
class ObjectIdGenerator {
    std::atomic<uint32_t> next_id {1};

public:
    uint32_t allocate() noexcept {
        return next_id.fetch_add(1, std::memory_order_relaxed);
    }
};
```

* 单调递增
* 无锁
* 允许 wrap（分析工具可识别时间段）

---

## 2️⃣ Stack 捕获

推荐：

### Linux

```cpp
backtrace()
```

### Windows

```cpp
RtlCaptureStackBackTrace()
```

封装为：

```cpp
struct StackCapture {
    void* frames[16];
    uint8_t depth;
};
```

注意：

* 不用 std::source_location（太重）
* 直接存 return address
* 分析阶段用 addr2line / dbghelp 解析

---

## 3️⃣ TraceEvent（写入 RingBuffer）

```cpp
enum class EventType : uint8_t {
    Alloc,
    Free
};

struct TraceEvent {
    EventType type;
    uint32_t object_id;
    StackCapture stack;
};
```

注意：

* Free 不需要 stack（可选）
* 结构固定大小，避免堆分配

---

# 四、Lock-Free Ring Buffer

单生产者多生产者均可。

推荐：

* 每线程一个本地 buffer
* 再汇总到全局 writer

或：

* MPMC ring buffer（固定容量）

示意：

```cpp
template<size_t N>
class RingBuffer {
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    TraceEvent buffer[N];
};
```

业务线程：

```
push(event)
```

后台线程：

```
while(true)
  pop batch
  process
```

---

# 五、后台 TraceWriter 线程

职责：

1. stack 去重
2. 分配 stack_id
3. 写 mmap 文件

---

## 1️⃣ Stack Interning

```cpp
struct StackKey {
    void* frames[16];
    uint8_t depth;
};

unordered_map<StackKey, uint32_t> stack_table;
```

hash：

* FNV-1a 或 xxhash
* 对 frames + depth 计算

如果已存在：

```
reuse stack_id
```

否则：

```
new stack_id
写入 stack section
```

---

# 六、文件格式设计（极关键）

建议使用 **顺序 append-only 二进制格式**

```
[Header]

[StackSection]
    stack_id
    depth
    frame0
    frame1
    ...

[ObjectEventSection]
    event_type
    object_id
    stack_id
```

---

## 文件结构示意

```
| MAGIC | VERSION |

| STACK_RECORD |
| STACK_RECORD |
| STACK_RECORD |

| EVENT_RECORD |
| EVENT_RECORD |
| EVENT_RECORD |
```

---

## StackRecord

```cpp
struct StackRecord {
    uint32_t stack_id;
    uint8_t depth;
    void* frames[depth];
};
```

---

## EventRecord

```cpp
struct EventRecord {
    uint8_t type;
    uint32_t object_id;
    uint32_t stack_id;
};
```

---

# 七、mmap 写入策略（推荐）

启动时：

1. 预分配 1GB 文件
2. `mmap`
3. 顺序写入
4. 记录当前 offset

优点：

* 无频繁 write syscall
* 顺序写性能极高
* 崩溃时大概率保留数据

周期性：

```
msync()
```

---

# 八、崩溃时策略

不要：

* 在 signal handler 里写文件
* malloc
* 加锁

崩溃时只：

```
printf("LEAK object_id=%u\n", id);
```

分析全靠 trace.log。

---

# 九、离线分析工具结构

```
trace_analyzer trace.log
```

步骤：

1. 读 stack 表

2. 读 event 表

3. 重建 object 生命周期

4. 找：

   * 未 free 的 object
   * 重复分配
   * 泄露统计
   * 热点 stack

5. 用：

   * addr2line (Linux)
   * dbghelp (Windows)

符号化地址

---

# 十、内存占用评估

运行时：

```
4 bytes × object_count
+ ring buffer
+ stack_table (unique stacks)
```

假设：

* 100万对象
* 2000 unique stacks

运行时内存：

```
4MB
+ 几百KB stack table
```

几乎可以忽略。

---

# 十一、性能特征

| 操作        | 成本        |
| --------- | --------- |
| 分配 ID     | 1 原子操作    |
| 捕获栈       | ~50-200ns |
| push ring | 无锁 O(1)   |
| 写文件       | 后台线程      |

业务线程不会被 IO 阻塞。

---

# 十二、可选增强

### 1️⃣ 每线程 stack cache

减少 hash 计算。

---

### 2️⃣ 分层模式

```
TRACE_LIGHT   → 只记录 stack_id
TRACE_FULL    → 记录完整栈
TRACE_OFF
```

---

### 3️⃣ 热点聚类分析

统计：

```
stack_id -> active_count
```

直接输出：

```
Top leaking stacks:
#1  count=23451
#2  count=10345
```

---

# 十三、最终结构总结图

```
[Object]
   ↓
object_id (4B)
   ↓
TraceEvent
   ↓
RingBuffer
   ↓
TraceWriter
   ↓
StackIntern
   ↓
mmap file
   ↓
Offline Analyzer
   ↓
Symbolication
   ↓
Human-readable leak report
```

---

# 结论

这个架构具备：

✔ 工业级稳定性
✔ O(1) 运行时成本
✔ 极低内存占用
✔ 可扩展
✔ 崩溃可恢复
✔ 可统计热点

这已经是一个“小型内存分析器”的完整形态。

---

如果你愿意，我可以继续给你：

* 精确的 lock-free ring buffer 设计（避免 ABA）
* StackKey 的高性能 hash 实现
* 文件格式的 crash-consistent 设计
* Windows/Linux 双平台实现差异说明
