# ECS Render System SKILL合集

本目录包含9个SKILL文档，涵盖HGL自有库类型参考、CMCoreType底层库完整参考、HGL日志系统、文件系统与IO流、添加新Component/System、系统分组、ExecutionPhase、RenderGraph和快速参考。

## ⚠️ 首先必读

在编写任何代码前，请先查阅 [SKILL_HGL_TYPES_REFERENCE.md](SKILL_HGL_TYPES_REFERENCE.md)，确保使用HGL自有库而非STL。

---

## 🎯 SKILL导航

### 0. [SKILL_HGL_TYPES_REFERENCE.md](SKILL_HGL_TYPES_REFERENCE.md)
**适用：查找字符串、集合、哈希、IO等基础类型的正确HGL用法**

- 📋 STL → HGL 完整替换对照表
- 🔤 `AnsiString`/`UTF8String`/`OSString` 使用示例
- 📦 `ArrayList`/`UnorderedMap`/`SortedSet` 使用示例
- 🔑 FNV1a哈希正确用法（禁止手写）
- 📁 IO读写完整示例
- ❌ 常见错误 vs ✅ 正确写法对比

**快速导航：**
```
需要字符串操作？
需要动态数组/Map？
需要哈希计算？
需要文件IO？
→ 使用这个SKILL
```

---

### 0.2 [SKILL_LOGGING_REFERENCE.md](SKILL_LOGGING_REFERENCE.md)
**适用：输出日志信息（替代 printf/cout/cerr）**

- 🪵 4种日志模式：OBJECT_LOGGER / GLogXxx / MLogXxx / FLogXxx
- 📋 日志级别对照表（Verbose/Debug/Info/Notice/Warning/Error/Fatal）
- 💡 各模式完整代码示例
- ❌ 常见错误（错误使用 printf / LOG_INFO 等不存在宏）vs ✅ 正确写法

**快速导航：**
```
类方法中需要输出日志？         → OBJECT_LOGGER + LogInfo/LogError
全局函数/main中输出日志？      → GLogInfo/GLogError
多类共享模块名输出日志？        → DEFINE_LOGGER_MODULE + MLogInfo
单文件自由函数输出日志？        → USE_MODULE_LOGGER + FLogInfo
→ 使用这个SKILL
```

---

### 0.3 [SKILL_FILESYSTEM_IO_REFERENCE.md](SKILL_FILESYSTEM_IO_REFERENCE.md)
**适用：文件系统操作、IO流读写（替代 std::ifstream / std::ofstream / std::filesystem）**

- 📁 FileSystem.h：文件增删改查、整体加载/保存、目录创建/删除
- 🗂️ Filename.h / Path 类：路径拼接、扩展名操作、规范化
- 🔍 CollectFiles / EnumFile：模式匹配文件枚举、递归遍历
- 📖 FileInputStream / FileOutputStream：RAII 辅助类用法
- 💾 MemoryInputStream / MemoryOutputStream：内存缓冲区流
- 🔢 DataInputStream / DataOutputStream：类型化结构体读写
- 📝 TextInputStream / LoadStringFromTextFile：文本逐行解析
- 🗺️ MMapFile：内存映射文件
- ❌ 常见错误（不存在的 GetFileSize / MakeDirectory / OpenFileInputStream函数等）

**快速导航：**
```
读文件？             → OpenFileInputStream（RAII类）
写文件？             → CreateFileOutputStream 或 OpenFileOutputStream
内存流？             → MemoryOutputStream / MemoryInputStream
路径操作？           → Filename.h 自由函数 或 Path 类
文件枚举/收集？      → CollectFiles（通配符/正则）或 EnumFile（继承扩展）
加载文本文件？       → hgl::LoadStringFromTextFile
内存映射大文件？     → OpenMMapFileOnlyRead / OpenMMapFile
→ 使用这个SKILL
```

---
**适用：查找CMCoreType底层库的基础类型、内存/对齐/枚举工具、数学、颜色、时间常量等**
- 🧹 内存安全宏（SAFE_CLEAR, SAFE_FREE 等）
- 🏗️ 平台宏（NO_COPY, NO_MOVE, OS/CPU 检测）
- ⚙️ 对齐工具（align_to, align_up, divide_ceil）
- 📐 MemoryAlloc / ObjectUtil（zero_new, construct_at, destroy_at）
- 🔢 枚举工具（ENUM_CLASS_RANGE, ToInt, RangeCheck, ENUM_CLASS_FOR）
- 📊 常量（HGL_SIZE_1KB, HGL_U32_MAX, HGL_TIME_ONE_DAY 等）
- 🎨 颜色类型（Color4f, Color3f, Color4ub, COLOR 枚举）
- 🧮 数学工具（Clamp, IsNearlyZero, IsNaN, IsValid, PhysicsConstants）
- 🕐 时间常量（Weekday, Month, HGL_TIME_* 系列）
- 🎯 事件系统（EventFunc, Property, DefEvent, SetEventCall）

**快速导航：**
```
需要基础类型 int8/uint8/f32/i64？
需要 SAFE_CLEAR / SAFE_FREE？
需要 NO_COPY / NO_MOVE 宏？
需要对齐计算 align_to / align_up？
需要枚举工具 ToInt / RangeCheck？
需要颜色类型 Color4f？
需要浮点验证 IsNaN / IsValid？
→ 使用这个SKILL
```

---

### 0.4 [SKILL_CMCORE_REFERENCE.md](SKILL_CMCORE_REFERENCE.md)
**适用：CMCore基础库中的线程/并发、时间、字符编码、队列/栈/LRU缓存、内存管理、智能指针等**

- 🧵 线程：`Thread`、`ThreadMutex`、`RWLock`、`Semaphore`、`CondVar`
- ⚛️ 原子：`atom<T>` / `atom_int` / `atom_bool`（替代 `std::atomic`）
- 🔄 线程安全集合：`SwapData`、`SwapList`、`SemSwapData`、`RingBuffer`
- ⏱️ 时间：`GetTimeSec`、`GetUptimeSec`、`SleepSecond`、`Timestamp` 类
- 🔤 字符编码：`Endian`（字节序）、`Charset`（字符集转换）、`utf.h`（UTF-8↔UTF-16）
- 📦 集合扩展：`Queue<T>`、`Stack<T>`、`LRUCache`、`FlatOrderedMap`、`OrderedMap`
- 💾 内存管理：`MemoryBlock`、`BlockAllocator`、`SharedPtr`/`WeakPtr`
- 📋 STL→HGL线程/时间速查表（位于文档末尾）

**快速导航：**
```
需要线程？                → Thread + ThreadMutex / RWLock
需要原子变量？            → atom<T> / atom_int / atom_bool
需要线程安全队列交换？    → SwapData / SemSwapData / SwapList
需要时间戳/计时？         → GetUptimeSec() / Timestamp
需要 UTF-8/UTF-16 转换？  → utf.h: to_u16 / to_u8 / ToOSString
需要队列/栈/LRU缓存？     → Queue<T> / Stack<T> / LRUCache<K,V>
需要有序Map高频增删？      → OrderedMap（B树）
需要静态有序Map序列化？   → FlatOrderedMap
需要内存块管理？          → MemoryBlock / BlockAllocator
→ 使用这个SKILL
```

---

### 1. [SKILL_ADD_NEW_RENDER_COMPONENT.md](SKILL_ADD_NEW_RENDER_COMPONENT.md)
**适用：首次添加新的渲染元素类型**

- ✅ 详细的5步工作流
- 💻 完整的代码模板
- ✓ 全面的CheckList
- 🚀 SkySphere示例
- ⏱️ 预计60-90分钟完成

**快速导航：**
```
需要添加 SkySphere？
需要添加 Particle？
需要添加 Decal？
→ 使用这个SKILL
```

---

### 2. [SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md](SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md)
**适用：理解系统分组和按名称启用/禁用机制**

- 🔍 Element Type概念
- 📚 API参考
- 🎮 3个完整场景示例
- 🔧 诊断和调试方法
- 📋 最佳实践

**快速导航：**
```
想按内容类型启用/禁用系统？
想实现质量预设切换？
想理解系统组结构？
→ 使用这个SKILL
```

---

### 3. [SKILL_EXECUTION_PHASE_ORDERING.md](SKILL_EXECUTION_PHASE_ORDERING.md)
**适用：选择ExecutionPhase和管理系统执行顺序**

- 📊 当前35+ Phase的完整结构图
- 🔄 执行流程详解
- 📋 选择Phase的决策表
- ⚠️ 常见顺序问题排查
- 🎯 依赖关系管理

**快速导航：**
```
新系统应该选什么Phase？
系统执行顺序不对怎么办？
多系统如何声明依赖？
→ 使用这个SKILL
```

---

### 4. [SKILL_RENDERGRAPH_USAGE.md](SKILL_RENDERGRAPH_USAGE.md)
**适用：创建和使用RenderGraph、定义渲染流程**

- 🎨 RenderGraph基础概念
- 📦 4个预设工厂详解
- 🛠️ 3个自定义图示例（多RT、质量预设、分层）
- 🎚️ Pass执行标志详解
- 🔨 动态切换和调试

**快速导航：**
```
需要多RT延迟渲染？
需要质量预设？
需要自定义渲染流程？
→ 使用这个SKILL
```

---

### 5. [SKILL_QUICK_REFERENCE.md](SKILL_QUICK_REFERENCE.md)
**适用：快速查找、模板复制、Checklist**

- ⚡ 任务-SKILL映射表
- ✅ 完整Checklist（总耗时60-90分钟）
- 💾 最小化代码模板
- 🔍 常用API速查
- 🆘 常见错误速查
- 📍 文件位置快查

**快速导航：**
```
我需要快速完成某项工作
我想复制代码模板
我需要检查Checklist
→ 使用这个SKILL
```

---

## 🚀 快速开始：3分钟内添加新元素

### 第一次？
1. 打开 [SKILL_QUICK_REFERENCE.md](SKILL_QUICK_REFERENCE.md) 第"添加新Component/System的标准流程"
2. 按Checklist逐项完成
3. 用代码模板替换占位符
4. 编译测试

### 需要细节？
- 代码写不对？→ [SKILL_ADD_NEW_RENDER_COMPONENT.md](SKILL_ADD_NEW_RENDER_COMPONENT.md)
- Phase选择不对？ → [SKILL_EXECUTION_PHASE_ORDERING.md](SKILL_EXECUTION_PHASE_ORDERING.md)
- 系统启用不了？ → [SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md](SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md)

---

## 📊 SKILL使用流程

```
新项需求
    ↓
选择合适的SKILL
    ↓
    ├─ 第一次做这件事？
    │  └─ SKILL_QUICK_REFERENCE (5 min) → 详细SKILL (20 min)
    │
    ├─ 需要代码示例？
    │  └─ SKILL_ADD_NEW_RENDER_COMPONENT (模板)
    │
    ├─ 需要理解原理？
    │  └─ 对应SKILL (深入) + 源码参考
    │
    └─ 遇到问题？
       └─ SKILL_QUICK_REFERENCE (常见错误) 
          或对应主题SKILL (诊断)
    ↓
完成实现+测试
```

---

## 📚 按SKILL的内容分类

### 📋 工作流指南
- **SKILL_ADD_NEW_RENDER_COMPONENT.md** - 添加新元素的完整流程
- **SKILL_QUICK_REFERENCE.md** - 速度优先的Checklist

### 🔬 原理和概念
- **SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md** - 系统分组机制
- **SKILL_EXECUTION_PHASE_ORDERING.md** - 执行阶段设计
- **SKILL_RENDERGRAPH_USAGE.md** - 图配置原理

### 🛠️ 实践和API
- **SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md** - API和场景示例
- **SKILL_RENDERGRAPH_USAGE.md** - RenderGraph API和预设
- **SKILL_QUICK_REFERENCE.md** - 代码模板和速查表

---

## 🎓 学习顺序建议

### 🟢 初学者路线（2小时）
1. 阅读 [SKILL_HGL_TYPES_REFERENCE.md](SKILL_HGL_TYPES_REFERENCE.md) - 5 min（**必读**）
2. 阅读 [SKILL_QUICK_REFERENCE.md](SKILL_QUICK_REFERENCE.md) - 5 min
3. 完成第一个元素（用Checklist）- 45 min
4. 阅读 [SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md](SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md) - 15 min
5. 尝试3个不同Element Type - 45 min

### 🟡 进阶路线（3小时）
1. 阅读所有6个SKILL - 60 min
2. 创建多系统元素（如Particle例子）- 60 min
3. 尝试自定义RenderGraph和质量预设 - 60 min

### 🔴 高级优化（额外）
- 性能分析（帧率下降时自动降级）
- 多RT延迟渲染
- 从配置文件加载质量预设

---

## 🔗 与项目其他文件的关系

```
.ai/skills/ 目录结构
├── SKILL_*.md (本集合，7个文件)
│   ├── SKILL_HGL_TYPES_REFERENCE.md  ← HGL类型参考（必读）
│   ├── SKILL_CMCORETYPE_REFERENCE.md ← CMCoreType底层库完整参考
│   ├── SKILL_CMCORE_REFERENCE.md     ← CMCore完整参考（线程/时间/队列等）
│   ├── SKILL_ADD_NEW_RENDER_COMPONENT.md
│   ├── SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md
│   ├── SKILL_EXECUTION_PHASE_ORDERING.md
│   ├── SKILL_RENDERGRAPH_USAGE.md
│   ├── SKILL_QUICK_REFERENCE.md
│   └── SKILL_INDEX.md (本文件)
│
└── 未来扩展 (计划)
    ├── SKILL_PARTICLE_SYSTEM_IMPL.md
    ├── SKILL_DECAL_SYSTEM_IMPL.md
    ├── SKILL_TERRAIN_RENDERING.md
    └── SKILL_DEFERRED_RENDERING.md

源码位置 (参考)
├── CMCore/inc/hgl/          ← 基础库（极少修改）
│   ├── thread/              ← 线程/并发
│   ├── time/                ← 时间
│   ├── type/                ← 集合/内存/智能指针
│   ├── io/                  ← IO流
│   └── filesystem/          ← 文件系统
├── inc/hgl/ecs/core/
│   ├── Component.h
│   ├── System.h (ExecutionPhase定义)
│   ├── Context.h (API)
│   └── RenderGraph.h (数据结构)
├── src/ecs/core/
│   ├── RenderGraph.cpp (实现)
│   └── DefaultSystemsCP.cpp (注册)
└── src/ecs/systems/render/ (系统实现们)
```

---

## ❓ 常见问题

**Q: 应该从哪个SKILL开始？**  
A: 先阅读 [SKILL_HGL_TYPES_REFERENCE.md](SKILL_HGL_TYPES_REFERENCE.md)（了解禁止使用STL的规则），再从 [SKILL_QUICK_REFERENCE.md](SKILL_QUICK_REFERENCE.md) 开始。

**Q: 能否离线使用这些SKILL？**  
A: 可以。所有SKILL都是本地markdown文件，使用VS Code直接打开即可。

**Q: SKILL会更新吗？**  
A: 会的。当新增系统类型或改变架构时会更新。检查版本日期。

**Q: 能否为我的项目定制SKILL？**  
A: 可能。这些SKILL针对ULRE的ECS系统设计。如果修改了架构，部分内容需要调整。

---

## 📝 SKILL元数据

| 文件 | 深度 | 时长 | 难度 | 实践 |
|------|------|------|------|------|
| SKILL_HGL_TYPES_REFERENCE.md | 📊 浅 | 5 min | ⭐ | ✓ |
| SKILL_CMCORETYPE_REFERENCE.md | 📊 深 | 10 min | ⭐ | ✓ |
| SKILL_CMCORE_REFERENCE.md | 📊 深 | 10 min | ⭐ | ✓ |
| SKILL_QUICK_REFERENCE.md | 📊 浅 | 5 min | ⭐ | ✓ |
| SKILL_ADD_NEW_RENDER_COMPONENT.md | 📘 深 | 20 min | ⭐⭐ | ✓✓ |
| SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md | 📗 中等 | 15 min | ⭐⭐ | ✓ |
| SKILL_EXECUTION_PHASE_ORDERING.md | 📙 中等 | 10 min | ⭐ | ✗ |
| SKILL_RENDERGRAPH_USAGE.md | 📕 中等 | 15 min | ⭐⭐ | ✓ |

---

## 🎯 关键主题快速链接

| 主题 | 相关SKILL | 快速用法 |
|------|---------|---------|
| Thread / 线程           | CMCORE_REFERENCE | §1.2 Thread |
| ThreadMutex / 互斥锁   | CMCORE_REFERENCE | §1.3 ThreadMutex |
| RWLock / 读写锁         | CMCORE_REFERENCE | §1.4 RWLock |
| Semaphore / 信号量      | CMCORE_REFERENCE | §1.5 Semaphore |
| CondVar / 条件变量      | CMCORE_REFERENCE | §1.6 CondVar |
| SwapData / 双缓冲交换   | CMCORE_REFERENCE | §1.7 SwapData |
| SwapList/SemSwapList    | CMCORE_REFERENCE | §1.8 SwapColl |
| atom<T> / 原子变量      | CMCORE_REFERENCE | §1.1 Atomic |
| GetTimeSec/GetUptimeSec | CMCORE_REFERENCE | §2.1 Time.h |
| Timestamp 时间戳类      | CMCORE_REFERENCE | §2.2 Timestamp |
| SleepSecond             | CMCORE_REFERENCE | §2.1 Time.h |
| EndianSwap/BOM检测      | CMCORE_REFERENCE | §3.1 Endian |
| to_u16/to_u8/ToOSString | CMCORE_REFERENCE | §3.3 utf.h |
| Queue<T>                | CMCORE_REFERENCE | §4.1 Queue |
| Stack<T>                | CMCORE_REFERENCE | §4.2 Stack |
| LRUCache                | CMCORE_REFERENCE | §4.3 LRUCache |
| FlatOrderedMap          | CMCORE_REFERENCE | §4.4 FlatOrderedMap |
| FlatOrderedSet          | CMCORE_REFERENCE | §4.5 FlatOrderedSet |
| OrderedMap（B树）        | CMCORE_REFERENCE | §4.6 OrderedMap |
| OrderedSet（B树）        | CMCORE_REFERENCE | §4.7 OrderedSet |
| AnsiStringView          | CMCORE_REFERENCE | §4.8 StringView |
| MemoryBlock             | CMCORE_REFERENCE | §5.1 MemoryBlock |
| BlockAllocator          | CMCORE_REFERENCE | §5.2 BlockAllocator |
| SharedPtr/WeakPtr       | CMCORE_REFERENCE | §6 Smart |
| EnumFile（递归枚举）     | CMCORE_REFERENCE | §8.1 EnumFile |
| TextInputStream逐行     | CMCORE_REFERENCE | §9.1 TextInputStream |
| TextOutputStream        | CMCORE_REFERENCE | §9.2 TextOutputStream |
| LoadStringFromTextFile  | CMCORE_REFERENCE | §9.3 LoadString |
| 字符串/AnsiString        | HGL_TYPES_REFERENCE | 字符串章节 |
| 动态数组/集合 | HGL_TYPES_REFERENCE | 集合章节 |
| FNV1a哈希 | HGL_TYPES_REFERENCE | 哈希章节 |
| 文件IO | HGL_TYPES_REFERENCE | IO章节 |
| 基础类型(int8/f32等) | CMCORETYPE_REFERENCE | §1 CoreType |
| SAFE_CLEAR/SAFE_FREE | CMCORETYPE_REFERENCE | §2 Macro |
| NO_COPY/NO_MOVE | CMCORETYPE_REFERENCE | §3 Platform |
| align_to/align_up | CMCORETYPE_REFERENCE | §4 AlignUtil |
| zero_new/new_copy | CMCORETYPE_REFERENCE | §5 MemoryAlloc |
| construct_at/destroy_at | CMCORETYPE_REFERENCE | §6 ObjectUtil |
| ENUM_CLASS_RANGE/ToInt | CMCORETYPE_REFERENCE | §7 EnumUtil |
| HGL_SIZE_1KB/HGL_U32_MAX | CMCORETYPE_REFERENCE | §8 Constants |
| numeric_max/numeric_min | CMCORETYPE_REFERENCE | §9 TypeLimits |
| GetMipLevel | CMCORETYPE_REFERENCE | §10 MipmapUtil |
| Clamp/ClampU8 | CMCORETYPE_REFERENCE | §11.1 ClampUtil |
| IsNearlyZero/IsNearlyEqual | CMCORETYPE_REFERENCE | §11.2 FloatPrecision |
| IsNaN/IsValid | CMCORETYPE_REFERENCE | §11.3 FloatValidation |
| 时间常量/Weekday/Month | CMCORETYPE_REFERENCE | §12 TimeConst |
| Color4f/Color3f/COLOR | CMCORETYPE_REFERENCE | §13 Color |
| FindDataPositionInArray | CMCORETYPE_REFERENCE | §14 ArrayItemProcess |
| ComputeOptimalHash | CMCORETYPE_REFERENCE | §17 QuickHash |
| HashMergeGolden64 | CMCORETYPE_REFERENCE | §17 HashMerge |
| DefEvent/SetEventCall | CMCORETYPE_REFERENCE | §18 EventFunc |
| Property<T> | CMCORETYPE_REFERENCE | §18 Property |
| OBJECT_LOGGER（类内日志） | LOGGING_REFERENCE | 模式1 |
| GLogInfo/GLogError（全局） | LOGGING_REFERENCE | 模式2 |
| DEFINE_LOGGER_MODULE（模块定义） | LOGGING_REFERENCE | 模式3 |
| EXTERN_LOGGER_MODULE（模块声明） | LOGGING_REFERENCE | 模式3 |
| MLogInfo/MLogError（模块日志） | LOGGING_REFERENCE | 模式3 |
| USE_MODULE_LOGGER（文件绑定） | LOGGING_REFERENCE | 模式4 |
| FLogInfo/FLogError（文件级日志） | LOGGING_REFERENCE | 模式4 |
| 日志级别选择 | LOGGING_REFERENCE | §日志级别 |
| FileExist/FileDelete/FileCopy | FILESYSTEM_IO_REFERENCE | §1 FileSystem.h |
| LoadFileToMemory/SaveMemoryToFile | FILESYSTEM_IO_REFERENCE | §1 FileSystem.h |
| MakePath/DeletePath/DeleteTree | FILESYSTEM_IO_REFERENCE | §1 FileSystem.h |
| GetFileInfo/FileInfo.size | FILESYSTEM_IO_REFERENCE | §1 FileSystem.h |
| GetFilename/GetExtension/GetStem | FILESYSTEM_IO_REFERENCE | §2 Filename.h |
| JoinPathWithFilename/SplitPath | FILESYSTEM_IO_REFERENCE | §2 Filename.h |
| Path 类（/ 运算符拼路径） | FILESYSTEM_IO_REFERENCE | §2 Path |
| CollectFiles（通配符/正则收集） | FILESYSTEM_IO_REFERENCE | §3 CollectFiles |
| EnumFile（递归枚举继承） | FILESYSTEM_IO_REFERENCE | §3 EnumFile |
| OpenFileInputStream（RAII读文件） | FILESYSTEM_IO_REFERENCE | §5 FileInputStream |
| CreateFileOutputStream（工厂写文件） | FILESYSTEM_IO_REFERENCE | §5 FileOutputStream |
| MemoryOutputStream/GetData/Tell | FILESYSTEM_IO_REFERENCE | §6 MemoryStream |
| DataInputStream/ReadInt32/ReadFloat | FILESYSTEM_IO_REFERENCE | §7 DataInputStream |
| DataOutputStream/WriteInt32/WriteFloat | FILESYSTEM_IO_REFERENCE | §7 DataOutputStream |
| LoadStringFromTextFile | FILESYSTEM_IO_REFERENCE | §8 文本IO |
| LoadStringListFromTextFile | FILESYSTEM_IO_REFERENCE | §8 文本IO |
| TextInputStream（逐行解析） | FILESYSTEM_IO_REFERENCE | §8 TextInputStream |
| RandomAccessFile（读写共用） | FILESYSTEM_IO_REFERENCE | §9 RandomAccessFile |
| OpenMMapFileOnlyRead/MMapFile | FILESYSTEM_IO_REFERENCE | §10 MMapFile |
| 创建新Component | ADD_NEW_RENDER_COMPONENT | 第2步 |
| 创建新System | ADD_NEW_RENDER_COMPONENT | 第3步 |
| SetRenderElementType() | SYSTEM_GROUPING_AND_ENABLEMENT | API参考 |
| SetElementTypeSystemsEnabled() | SYSTEM_GROUPING_AND_ENABLEMENT | API参考 |
| CreateAdaptiveRenderGraph() | RENDERGRAPH_USAGE | 预设工厂 |
| 选择ExecutionPhase | EXECUTION_PHASE_ORDERING | 决策表 |
| 多系统依赖关系 | EXECUTION_PHASE_ORDERING | 原则1 |
| 质量预设 | SYSTEM_GROUPING_AND_ENABLEMENT | 场景2 |
| 自定义RenderGraph | RENDERGRAPH_USAGE | 场景1-3 |
| 代码模板 | QUICK_REFERENCE | 代码模板速查 |
| Checklist | QUICK_REFERENCE | 完整Checklist |

---

## 📞 需要帮助？

如果遇到问题：

1. **检查SKILL_QUICK_REFERENCE的"常见错误"部分** - 可能已解决
2. **查看对应SKILL的"常见问题"部分** - 查找答案
3. **参考源码示例** - 每个SKILL都列出了参考代码位置
4. **调试和诊断** - 参考对应SKILL的诊断部分

---

## ✨ 总结

这9个SKILL文档提供了从零到精通的**完整路径**，支持：

- ✅ 快速上手（QUICK_REFERENCE）
- ✅ 详细学习（各topic SKILL）
- ✅ 代码复制（模板）
- ✅ 问题解决（常见错误、诊断）
- ✅ 深度理解（原理篇）
- ✅ 实践项目（示例场景）

**立即开始：** 打开 [SKILL_QUICK_REFERENCE.md](SKILL_QUICK_REFERENCE.md) 的Checklist部分。

祝你的新元素实现顺利！ 🚀
