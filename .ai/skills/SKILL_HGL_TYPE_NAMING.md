# HGL 类型与容器命名规范

本规范约束 `CMCoreType/inc/hgl/type` 与 `CMCore/inc/hgl/type` 下的容器与基础类型，
并为新代码给出统一命名约定。**不做大规模重命名**：既有 API 保持兼容，新代码遵守本规范。

---

## 1. 三条命名带

| 类别 | 风格 | 示例 |
|---|---|---|
| STL 兼容成员 | 小写 | `begin()` `end()` `size()` `empty()` `data()` `push_back()` |
| 容器业务 API | PascalCase | `GetCount()` `GetData()` `Add()` `Delete()` `Insert()` `Move()` |
| 自由函数/工具 | snake_case | `mem_copy()` `construct_at_copy()` `destroy_range()` |

判断依据是"是否与标准库同名"：与 STL 同名者保持小写，便于与标准算法/范围 for 互操作；
容器自有的业务语义 API 使用 PascalCase；底层内存工具函数使用 snake_case。

---

## 2. 容器尺寸与容量

- `GetCount()` / `GetAllocCount()` 为历史 API，返回 `int`，保留不变。
- `size()` / `empty()` 为 STL 风格别名，`size()` 返回 `size_t`。
- 字节数一律使用 `size_t`：`GetTotalBytes()`、`GetAllocBytes()`。
  禁止在内部把 `size * sizeof(T)` 强转成 `int`。

```cpp
ValueArray<int> a;
const size_t n = a.size();            // 推荐
const int    c = a.GetCount();        // 兼容旧代码
const size_t b = a.GetTotalBytes();   // 字节数
```

---

## 3. 删除语义

| API | 语义 | 复杂度 | 是否保序 |
|---|---|---|---|
| `Delete(index[,num])` | 删除并前移后续元素 | O(n) | 是 |
| `DeleteSwap(index)` | 用尾元素填补被删位置 | O(1) | 否 |
| `DeleteShift(index[,num])` | `Delete` 的历史别名 | O(n) | 是 |

新代码不要引入 `DeleteShift`；需要保序用 `Delete`，需要高性能用 `DeleteSwap`。

---

## 4. const 正确性

- **禁止** `const` 成员函数通过 `const_cast` 返回可写指针。
  提供 `const` / 非 `const` 两组重载：`T *GetData()` 与 `const T *GetData() const`。
- 值参数用 `const T &`，不仅是 `T &`；删除类接口用 `const T *`。
- 不再书写 `const int GetCount() const` 这类"返回值 const"（对标量无意义）。

---

## 5. 所有权

| 容器 | 元素要求 | 所有权 |
|---|---|---|
| `ValueArray<T>` | `is_trivially_copyable_v<T>` | 值语义 |
| `ValueArray<T[N]>` | 元素平凡可拷贝 | 值语义（对齐存储） |
| `PtrArray<T>` | `!is_trivially_copyable_v<T>` | 值语义，显式构造/析构 |
| `ManagedArray<T>` | 非平凡类型 | **拥有** `T*`，析构 `delete` |

`ManagedArray` 禁止拷贝（浅拷贝会双重释放），只提供移动语义；需要复制内容请在调用点显式创建对象。
`PtrArray::SetData()` 只接受 `allocate_raw_memory<T>()` 分配的内存。

---

## 6. 不可变性与失效规则

- `UndorderedMap`/`OrderedMap` 系列：插入、删除、rehash 都会使指针/引用/迭代器失效。
- `ValueArray`/`PtrArray`：插入、删除、扩容都会移动元素，之前取得的指针失效。
- `Queue`：`GetWriteArray()` 暴露写段，直接写入会绕过入队语义；只读请用 `GetUnreadSnapshot()`。

---

## 7. 字节级内存操作

- `mem_zero` / `mem_compare` / `mem_compare_ordering` 仅接受 trivially copyable 类型；
  确需绕过时显式使用 `mem_zero_unsafe` / `mem_compare_unsafe` 并注释原因。
- 含 padding 或指针的类型优先使用 `operator==` / `<=>`，不要用字节比较。

---

## 8. 新增 API 检查清单

1. 与标准库同名者用小写，否则 PascalCase。
2. 尺寸/字节数用 `size_t`。
3. `const` 正确，不用 `const_cast` 逃逸。
4. 需要保序/不保序时，明确选择 `Delete` 还是 `DeleteSwap`。
5. 涉及所有权时，先明确"拥有 / 借用"，再选择容器。
6. 新增行为必须附带 `CMCore/examples/datatype/collection/` 下的测试。
