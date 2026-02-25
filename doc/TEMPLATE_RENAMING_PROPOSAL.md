# 模板类重命名方案

## 概述
根据对现有模板类实现的深入分析，提出一套更加科学合理的命名方案。核心思路是**按照存储方式+数据类型特征**进行分类，使名称能直观表达容器的本质特点。

---

## 一级分类：存储方式

### 1. **按值存储**（Value-based Storage）
- 直接将元素内联存储在容器中
- 使用 memcpy/memmove 进行高效操作
- 仅限平凡可复制类型（trivially copyable）
- **前缀建议**：`Value` 或 `Inline`

### 2. **按指针存储**（Pointer-based Storage）
- 存储元素指针，需要手动或自动的生命周期管理
- 支持非平凡类型（non-trivial types）
- **前缀建议**：`Ptr`、`Managed` 或 `Owned`

---

## 二级分类：访问方式

### 1. **顺序访问**（Sequential Access）
- 按插入或移动的顺序存储和访问

### 2. **索引访问**（Index-based Access）
- 使用额外的索引数组来管理数据位置
- 数据本身位置固定，索引可以调整

### 3. **有序访问**（Ordered Access）
- 数据自动保持排序状态
- 不允许重复元素

---

## 重命名建议表

| 当前名称 | 建议新名称 | 分类 | 理由 |
|---------|----------|------|------|
| **DataArray** | **ValueBuffer** | Value-based, Sequential | 强调：(1) 按值存储的缓冲区；(2) 轻量、无额外管理开销；(3) 是其他容器的基础实现 |
| **ArrayList** | **ValueArray** | Value-based, Sequential | 强调：(1) 按值存储的动态数组；(2) 直接替代 std::vector；(3) 继承自 ValueBuffer |
| **IndexedList** | **IndexedList** | Value-based, Indexed | 强调：(1) 按值存储的索引数组；(2) 数据位置固定，通过索引访问；(3) 适合频繁增删排序 |
| **ObjectArray** | **PtrArray** | Pointer-based, Sequential | 强调：(1) 按指针存储；(2) 显式构造/析构管理；(3) 平凡类型用 ValueArray，非平凡类型用这个 |
| **ObjectList** | **OwnedPtrList** 或 **ManagedList** | Pointer-based, Sequential | 强调：(1) 拥有指针的所有权；(2) 析构时会 delete；(3) 自动内存管理 |
| **ObjectIndexedList** | **IndexedManagedArray** 或 **IndexedPtrArray** | Pointer-based, Indexed | 强调：(1) 按指针存储的索引数组；(2) 支持非平凡类型；(3) 对应 IndexedList |
| **SortedSet** | **OrderedSet** 或 **UniqueValueSet** | Value-based, Ordered | 强调：(1) 按值存储的有序集合；(2) 自动排序且去重；(3) 不允许重复元素 |
| **SortedObjectSet** | **OrderedManagedSet** 或 **UniqueManagedSet** | Pointer-based, Ordered | 强调：(1) 按指针存储的有序集合；(2) 支持非平凡类型；(3) 对应 OrderedSet |
| **SmallMap** | **ValueKVMap** 或 **InlineMap** | Value-based, Ordered | 强调：(1) 按值存储键值对；(2) 紧凑型实现，无指针开销；(3) 适合小数据集；(4) 保持有序（基于二叉搜索） |
| **ObjectManager** | **RefCountedObjectRegistry** 或 **IdObjectManager** | Pointer-based, Managed | 强调：(1) 自动引用计数；(2) 支持 ID 分配和自动递增；(3) 对象注册表；(4) 生命周期自动管理 |
| **ElementCollection** | **TypedCollection** | Value-based/Pointer-based | 强调：(1) 集合的模板化包装；(2) 泛型版本的 Collection；(3) 提供类型安全的接口 |
| **DataStackPool** | **SimpleValuePool** 或 **BufferPool** | Value-based, Pool | 强调：(1) 简单的对象池；(2) 基于栈的分配策略；(3) 固定大小，无动态扩展 |
| **DataChain** | **BlockAllocator** 或 **ChainedBlockAllocator** | Pointer-based, Allocation | 强调：(1) 链表式的块分配器；(2) 自动合并相邻空闲块；(3) 低碎片化的内存管理 |

---

## 命名规则总结

### 规则一：前缀约定
```
Value*   → 按值存储的平凡类型容器
Ptr*     → 按指针存储的容器（可选前缀）
Managed* → 按指针存储且自动管理生命周期
Owned*   → 拥有指针所有权并负责释放
Indexed* → 支持索引访问的容器
Ordered* → 保持有序且去重的容器
```

### 规则二：后缀约定
```
*Array    → 支持顺序访问的动态数组
*Buffer   → 轻量的缓冲区（通常是基础实现）
*List     → 支持灵活增删的列表
*Set      → 有序且去重的集合
*Map      → 键值对映射
*Pool     → 对象池
*Allocator → 内存分配器
*Registry → 注册表（带 ID 管理）
```

### 规则三：分类约定
```
Value + Indexed  → ValueIndexedArray（按值存储的索引数组）
Ptr + Indexed    → PtrIndexedArray（按指针存储的索引数组）
Managed + Pool   → ManagedPool（带生命周期管理的对象池）
```

---

## 对应关系详解

### 第一组：基础存储层
```
DataArray         →  ValueBuffer
  ├─ 轻量级缓冲区
  ├─ 支持 memcpy/memmove
  └─ 是其他容器的基础
```

### 第二组：顺序访问容器
```
ArrayList         →  ValueArray         (继承自 ValueBuffer)
ObjectArray       →  PtrArray           (对应的指针版本)
ObjectList        →  OwnedPtrList       (自动管理生命周期)
```

### 第三组：索引访问容器
```
IndexedList       →  IndexedList  (按值存储，索引访问)
ObjectIndexedList →  IndexedManagedArray(按指针存储，索引访问)
```

### 第四组：有序集合
```
SortedSet         →  OrderedSet    (按值存储，有序去重)
SortedObjectSet   →  OrderedManagedSet  (按指针存储，有序去重)
SmallMap          →  ValueKVMap         (按值存储，有序键值对)
```

### 第五组：管理型容器
```
ObjectManager     →  RefCountedObjectRegistry
                      或 IdObjectManager
                      (自动引用计数 + ID 分配)
```

### 第六组：池和分配器
```
DataStackPool     →  SimpleValuePool    (简单对象池)
DataChain         →  BlockAllocator     (链式块分配器)
```

---

## 优点分析

### 1. **即时识别**
- 看到 `Value*` 立即知道是按值存储，性能较好
- 看到 `Managed*` 立即知道有生命周期管理
- 看到 `Indexed*` 立即知道支持索引访问

### 2. **一致性**
- 同一类型的不同实现能通过命名体现关系
  - `ValueArray` ↔ `PtrArray`
  - `OrderedSet` ↔ `OrderedManagedSet`
  - `IndexedList` ↔ `IndexedManagedArray`

### 3. **可维护性**
- 新开发者能快速理解容器特性
- 文档和代码意图更清晰
- 选择容器时有清晰的决策依据

### 4. **性能指导**
- `Value*` 强调高效，适合频繁访问
- `Ptr*` 强调灵活，适合非平凡类型
- `Indexed*` 强调增删排序效率高
- `Ordered*` 强调查询效率高（二叉搜索）

---

## 迁移建议

### 阶段一：文档和注释
1. 在头文件中添加新的别名
   ```cpp
   // 在 DataArray.h 中
   template<typename T> 
   using ValueBuffer = DataArray<T>;
   ```

2. 更新类注释，说明新名称的含义

### 阶段二：API 和文档
1. 使用新名称编写示例代码
2. 更新 README 和指南文档
3. 在 GitHub Issues 中公布迁移计划

### 阶段三：代码库迁移
1. 逐步替换内部使用
2. 保留旧名称作为别名以确保兼容性
3. 在主版本升级时完全切换

---

## 可选方案对比

### 方案 A: 保守型（当前建议）
**优点**：清晰表达存储方式和特性  
**缺点**：名称可能略长  
**示例**：`ValueArray`, `IndexedManagedArray`, `OrderedSet`

### 方案 B: 激进型
使用更短的缩写
```
VA   → ValueArray         (缩写为 VA)
PA   → PtrArray           (缩写为 PA)
IVA  → IndexedList  (缩写为 IVA)
```
**优点**：名称简洁  
**缺点**：可读性下降，需要学习曲线

### 方案 C: 继承 STL 风格
```
vector<T>              → ValueArray (类似 std::vector)
vector<unique_ptr<T>>  → ManagedArray (对应)
```
**优点**：接近 STL，容易迁移  
**缺点**：无法表达所有特性（如 Indexed、Ordered）

---

## 结论

**推荐方案：方案 A（保守型）**

理由：
1. ✅ 命名清晰，自文档化（Self-documenting）
2. ✅ 分类体系完整，易于学习
3. ✅ 支持快速决策（选择哪个容器）
4. ✅ 兼容现有代码（可同时提供别名）
5. ✅ 便于扩展（新容器容易取名）

---

## 使用决策树

```
我要存储什么数据？
├─ 平凡类型（int, float, POD struct）
│  ├─ 需要顺序访问
│  │  └─ ValueArray
│  ├─ 需要索引访问（数据位置固定）
│  │  └─ IndexedList
│  ├─ 需要有序且去重
│  │  └─ OrderedSet
│  └─ 需要键值对映射
│     └─ ValueKVMap
│
├─ 非平凡类型（std::string, 自定义类）
│  ├─ 需要顺序访问
│  │  ├─ 我要管理生命周期
│  │  │  └─ OwnedPtrList
│  │  └─ 容器管理生命周期
│  │     └─ PtrArray
│  ├─ 需要索引访问
│  │  └─ IndexedManagedArray
│  ├─ 需要有序且去重
│  │  └─ OrderedManagedSet
│  └─ 需要 ID + 引用计数管理
│     └─ RefCountedObjectRegistry
│
└─ 特殊场景
   ├─ 高频申请/释放（对象池）
   │  └─ SimpleValuePool 或 ManagedPool
   └─ 内存碎片化问题
      └─ BlockAllocator
```

