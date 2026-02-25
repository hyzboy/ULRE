# 方案一：完整重命名清单（按规则统一）

## 核心命名规则

### 规则1：前缀区分存储方式和生命周期管理
```
Value*        → 按值存储（平凡类型）
Ptr*          → 按指针存储，用户自己管理生命周期（不拥有）
Managed*      → 按指针存储，容器自动管理生命周期（拥有）
```

### 规则2：后缀区分访问方式
```
*Buffer       → 基础缓冲区实现
*Array        → 顺序访问动态数组
*List         → 灵活增删列表（保留 List 用于强调语义）
*Set          → 有序去重集合
*KVMap        → 键值对映射
*Registry     → 对象注册表（带ID管理）
*Pool         → 对象池
*Allocator    → 内存分配器
```

### 规则3：复合名称构成
```
[Indexed|Ordered]? + [Value|Ptr|Managed] + [Array|Set|...]
示例：
  IndexedList    = Indexed + Value + Array
  OrderedManagedSet    = Ordered + Managed + Set
  ManagedObjectRegistry = Managed + Object + Registry
```

---

## 完整重命名对照表

### 第一组：基础缓冲区层

| 序号 | 当前名称 | 方案一新名称 | 原因 |
|-----|---------|-----------|------|
| 1 | `DataArray<T>` | `ValueBuffer<T>` | 轻量级缓冲区，是其他容器的基础 |

---

### 第二组：顺序访问容器（Value系列）

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 2 | `ArrayList<T>` | `ValueArray<T>` | ValueBuffer | 按值存储的动态数组 |

---

### 第三组：顺序访问容器（Ptr系列 - 用户管理）

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 3 | `ObjectArray<T>` | `PtrArray<T>` | 内部使用 DataArray | 存储指针，用户负责 delete |

---

### 第四组：顺序访问容器（Managed系列 - 自动管理）

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 4 | `ObjectList<T>` | `ManagedArray<T>` | 内部使用 ArrayList | **改为一致**：自动 delete；名称简洁清晰 |

---

### 第五组：索引访问容器（Value系列）

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 5 | `IndexedList<T>` | `IndexedList<T>` | DataArray | 按值存储，数据位置固定，通过索引访问 |

---

### 第六组：索引访问容器（Managed系列）

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 6 | `ObjectIndexedList<T>` | `IndexedManagedArray<T>` | ObjectArray | 已一致：非平凡类型索引访问，自动管理 |

---

### 第七组：有序集合（Value系列）

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 7 | `SortedSet<T>` | `OrderedSet<T>` | DataArray | 按值存储，自动排序去重 |

---

### 第八组：有序集合（Managed系列）

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 8 | `SortedObjectSet<T>` | `OrderedManagedSet<T>` | ObjectArray | 已一致：非平凡类型有序集合，自动管理 |

---

### 第九组：键值对映射（Value系列）

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 9 | `SmallMap<K,V>` | `ValueKVMap<K,V>` | ArrayList | 按值存储，紧凑型键值对映射 |

---

### 第十组：管理型容器（注册表）

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 10 | `ObjectManager<K,V>` | `ManagedObjectRegistry<K,V>` | 内部 MapTemplate | **改为一致**：带引用计数和ID管理的对象注册表 |

---

### 第十一组：包装容器

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 11 | `ElementCollection<T>` | `TypedCollection<T>` | Collection | Collection 的模板化包装，提供类型安全接口 |

---

### 第十二组：对象池

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 12 | `DataStackPool<T>` | `SimpleValuePool<T>` | DataArray + SeriesInt | 简单的栈式对象池，固定大小 |

---

### 第十三组：内存分配器

| 序号 | 当前名称 | 方案一新名称 | 继承/基础 | 说明 |
|-----|---------|-----------|---------|------|
| 13 | `DataChain` | `BlockAllocator` | DataStackPool + SortedSet | 链式块分配器，自动合并相邻空闲块 |

---

## 对应关系总览

### 顺序访问三系列

```
基础层：
  ValueBuffer<T>
    ↓
按值存储：
  ValueArray<T>
    ↓
按指针存储（用户管理）：        按指针存储（自动管理）：
  PtrArray<T>             ↔     ManagedArray<T>
```

### 索引访问两系列

```
按值存储：
  IndexedList<T>
    ↓
按指针存储（自动管理）：
  IndexedManagedArray<T>
```

### 有序集合两系列

```
按值存储：
  OrderedSet<T>
    ↓
按指针存储（自动管理）：
  OrderedManagedSet<T>
```

### 键值对系列

```
按值存储：
  ValueKVMap<K,V>
  
（无对应的指针版本，如需要可添加 ManagedKVMap<K,V>）
```

### 管理系列

```
对象注册表（带ID + 引用计数）：
  ManagedObjectRegistry<K,V>
```

---

## 改动统计

### 需要改动的类（4个）

| # | 旧名称 | 新名称 | 改动原因 |
|---|--------|--------|--------|
| 1 | `DataArray` | `ValueBuffer` | 更好地表达"缓冲区"的本质 |
| 2 | `ArrayList` | `ValueArray` | 与按值存储体系保持一致 |
| 3 | `ObjectList` | `ManagedArray` | **一致化**：统一用 `Managed` 表达自动管理 |
| 4 | `ObjectManager` | `ManagedObjectRegistry` | **一致化**：统一用 `Managed` 表达自动管理 |

### 已经一致的类（9个）

| # | 名称 | 所属系列 | 说明 |
|---|------|---------|------|
| 1 | `ObjectArray` | Ptr系列 | 无需改动，改为 `PtrArray` |
| 2 | `IndexedList` | Value系列 | 改为 `IndexedList` |
| 3 | `ObjectIndexedList` | Managed系列 | 保持 `IndexedManagedArray` |
| 4 | `SortedSet` | Value系列 | 改为 `OrderedSet` |
| 5 | `SortedObjectSet` | Managed系列 | 保持 `OrderedManagedSet` |
| 6 | `SmallMap` | Value系列 | 改为 `ValueKVMap` |
| 7 | `ElementCollection` | 包装层 | 改为 `TypedCollection` |
| 8 | `DataStackPool` | 工具层 | 改为 `SimpleValuePool` |
| 9 | `DataChain` | 工具层 | 改为 `BlockAllocator` |

---

## 关键改动点说明

### 1. ObjectList → ManagedArray

**原因**：
- 当前 `ObjectList` 使用 "Object" 前缀，与 `ObjectArray` 重复
- `ObjectIndexedList` 已经用 "Managed" 表达生命周期管理
- `SortedObjectSet` 也用 "Managed" 表达管理
- `ObjectArray` 改为 `PtrArray` 后，`ObjectList` 的 "Object" 前缀更加冗余

**改为 ManagedArray**：
- ✅ 与 `IndexedManagedArray` 和 `OrderedManagedSet` 体系一致
- ✅ 名称简洁：`Managed` 清晰表达"自动管理"
- ✅ 对标 `PtrArray`：一个需要用户管理，一个自动管理

### 2. ObjectManager → ManagedObjectRegistry

**原因**：
- "Object" 前缀含义不清（与 `ObjectArray`、`ObjectList` 冲突）
- 需要强调两个关键特性：自动管理 + ID/注册表功能

**改为 ManagedObjectRegistry**：
- ✅ `Managed` 表达生命周期管理和引用计数
- ✅ `Registry` 强调"注册表"功能
- ✅ 整体表意完整

### 3. 保留的改动

这些改动支持上述两个关键改动：
- `DataArray` → `ValueBuffer`：强调基础缓冲区角色
- `ArrayList` → `ValueArray`：与 `ManagedArray` 对称
- `ObjectArray` → `PtrArray`：简化名称，与 `Managed*` 系列区分

---

## 向后兼容方案

为了降低迁移成本，可以在头文件中添加别名：

```cpp
// 新名称
template<typename T> using ValueBuffer = DataArray<T>;
template<typename T> using ValueArray = ArrayList<T>;
template<typename T> using PtrArray = ObjectArray<T>;
template<typename T> using ManagedArray = ObjectList<T>;
template<typename T> using IndexedList = IndexedList<T>;
template<typename T> using IndexedManagedArray = ObjectIndexedList<T>;
template<typename T> using OrderedSet = SortedSet<T>;
template<typename T> using OrderedManagedSet = SortedObjectSet<T>;
template<typename K, typename V> using ValueKVMap = SmallMap<K,V>;
template<typename K, typename V> using ManagedObjectRegistry = ObjectManager<K,V>;
template<typename T> using TypedCollection = ElementCollection<T>;
template<typename T> using SimpleValuePool = DataStackPool<T>;
using BlockAllocator = DataChain;

// 保留原名称用于兼容
template<typename T> using DataArray = ValueBuffer<T>;
template<typename T> using ArrayList = ValueArray<T>;
// ... etc
```

---

## 使用决策树（基于新名称）

```
我要存储什么数据？
├─ 平凡类型（int, float, POD struct）
│  ├─ 需要顺序访问、频繁查找
│  │  └─ ValueArray<T>
│  ├─ 需要索引访问、数据位置固定
│  │  └─ IndexedList<T>
│  ├─ 需要有序且去重、二叉搜索查询
│  │  └─ OrderedSet<T>
│  └─ 需要键值对映射
│     └─ ValueKVMap<K,V>
│
├─ 非平凡类型（std::string, 自定义类）
│  ├─ 我要自己管理生命周期（delete）
│  │  └─ PtrArray<T>
│  │     （我知道我在做什么）
│  │
│  ├─ 容器帮我管理生命周期
│  │  ├─ 需要顺序访问、频繁查找
│  │  │  └─ ManagedArray<T>
│  │  ├─ 需要索引访问、数据位置固定
│  │  │  └─ IndexedManagedArray<T>
│  │  ├─ 需要有序且去重
│  │  │  └─ OrderedManagedSet<T>
│  │  └─ 需要 ID + 引用计数 + 注册表
│  │     └─ ManagedObjectRegistry<K,V>
│
└─ 特殊场景
   ├─ 频繁申请/释放，固定大小
   │  └─ SimpleValuePool<T>
   └─ 复杂的动态块分配、内存碎片化
      └─ BlockAllocator
```

---

## 总结

**方案一的一致性优势**：

✅ **清晰的前缀规则**
- `Value*` = 平凡类型，按值存储
- `Ptr*` = 非平凡类型，指针存储，用户管理
- `Managed*` = 非平凡类型，指针存储，自动管理

✅ **消除命名冗余**
- 四个 "Object" 前缀的类统一为明确的 `Ptr*` 或 `Managed*`
- 新用户一眼就能看出区别

✅ **对应关系清晰**
```
ValueArray      ↔ ManagedArray         （顺序访问）
IndexedList ↔ IndexedManagedArray （索引访问）
OrderedSet ↔ OrderedManagedSet    （有序访问）
```

✅ **支持扩展**
- 如需要 `ManagedKVMap<K,V>` 可直接添加
- 命名体系可轻松容纳新容器

