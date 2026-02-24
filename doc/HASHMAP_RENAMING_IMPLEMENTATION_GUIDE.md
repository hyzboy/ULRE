# HashMap 模板命名改进 - 实施指南

## 📋 任务概述

将 HashMap 系列模板类名改为与系统规则一致的新名称，提升代码的一致性和可维护性。

**改名映射表**：
| 当前名称 | 新名称 | 文件 |
|---------|--------|------|
| `HashMapTemplate<K,V,KVData>` | `ValueHashMap<K,V,KVData>` | HashMap.h |
| `HashMap<K,V>` | `ValueHashMap<K,V>` | HashMap.h |
| `ObjectHashMapTemplate<K,V,KVData>` | `ManagedHashMap<K,V,KVData>` | HashMap.h |
| `ObjectHashMap<K,V>` | `ManagedHashMap<K,V>` | HashMap.h |

---

## 🎯 步骤一：定位所有引用

### 1.1 在 HashMap.h 中搜索

打开 [HashMap.h](HashMap.h) 并查看：
- 所有类定义
- 类型别名（using）
- 模板特化

### 1.2 在整个项目中搜索引用

需要扫描以下文件以找出所有引用：

```bash
# 搜索 HashMap 的使用（在整个workspace）
# 预期结果：大多数在测试文件或示例中
HashMap<
ObjectHashMap<
```

**关键点**：
- HashMap.h 应该是主要改动点
- 其他文件如有使用，需要相应调整

---

## 🔧 步骤二：修改 HashMap.h

### 2.1 类定义改名

**改动位置 1**：基础模板类定义
```cpp
// 当前
template<typename K, typename V, typename KVData, int MAX_COLLISION = 4>
class HashMapTemplate
{
    // ...
};

// 改为
template<typename K, typename V, typename KVData, int MAX_COLLISION = 4>
class ValueHashMap
{
    // ...
};
```

**改动位置 2**：值存储版本的简化定义
```cpp
// 当前
template<typename K, typename V, int MAX_COLLISION = 4>
class HashMap : public HashMapTemplate<K, V, KeyValue<K, V>, MAX_COLLISION>
{
    // ...
};

// 改为
template<typename K, typename V, int MAX_COLLISION = 4>
class ValueHashMap : public ValueHashMap<K, V, KeyValue<K, V>, MAX_COLLISION>
{
    // ...
};
```

**⚠️ 注意**：简化版本的继承关系需要调整！

**改动位置 3**：对象哈希映射基础模板
```cpp
// 当前
template<typename K, typename V, typename KVData, int MAX_COLLISION = 4>
class ObjectHashMapTemplate : public HashMapTemplate<K, V*, KVData, MAX_COLLISION>
{
    // ...
};

// 改为
template<typename K, typename V, typename KVData, int MAX_COLLISION = 4>
class ManagedHashMap : public ValueHashMap<K, V*, KVData, MAX_COLLISION>
{
    // ...
};
```

**改动位置 4**：对象哈希映射简化版本
```cpp
// 当前
template<typename K, typename V, int MAX_COLLISION = 4>
class ObjectHashMap : public ObjectHashMapTemplate<K, V, KeyValue<K, V*>, MAX_COLLISION>
{
    // ...
};

// 改为
template<typename K, typename V, int MAX_COLLISION = 4>
class ManagedHashMap : public ManagedHashMap<K, V, KeyValue<K, V*>, MAX_COLLISION>
{
    // ...
};
```

**⚠️ 注意**：同样需要调整继承关系！

### 2.2 类型别名改名（如果有）

查找 using 声明：
```cpp
// 如果存在这样的声明，需要改名
using SomeHashMap = HashMap<int, std::string>;
// 改为
using SomeHashMap = ValueHashMap<int, std::string>;
```

### 2.3 模板特化改名

查找特化版本（特别是字符串哈希）：
```cpp
// 当前（示例）
template<int MAX_COLLISION>
class HashMapTemplate<const char*, const char*, KeyValue<const char*, const char*>, MAX_COLLISION>
{
    // ...
};

// 改为
template<int MAX_COLLISION>
class ValueHashMap<const char*, const char*, KeyValue<const char*, const char*>, MAX_COLLISION>
{
    // ...
};
```

---

## 🧪 步骤三：编译验证

### 3.1 清理构建
```bash
# 清理旧的构建文件（如果存在）
cd e:\ULRE\build
# 删除旧文件或使用 cmake 清理
```

### 3.2 重新编译
```bash
# 使用现有的构建系统编译
# 确保没有编译错误
```

### 3.3 检查编译错误

常见错误及解决方案：

| 错误 | 原因 | 解决方案 |
|------|------|---------|
| `'HashMapTemplate' is not a type` | 忘记改某处类名 | 使用"替换全文"确保改完 |
| `circular inheritance` | 继承关系调整错误 | 检查继承关系，确保没有循环 |
| 链接错误 | 模板定义不完整 | 确保所有定义都在头文件中 |

---

## 📝 步骤四：检查和调整继承关系

### 问题说明

当简化版本继承自基础版本时：
```cpp
// 问题示例（改名后会有问题）
template<typename K, typename V>
class ValueHashMap : public ValueHashMap<K, V, KeyValue<K, V>>  // ❌ 名称冲突
{
    // ...
};
```

### 解决方案

**方案 A**：给基础模板单独命名（推荐）
```cpp
// 基础模板保留 ValueHashMap（通用）
template<typename K, typename V, typename KVData, int MAX_COLLISION = 4>
class ValueHashMap
{
    // ...
};

// 简化版本直接继承，名称相同（C++ 允许这样的特化）
// 这是模板偏特化的常见模式
```

**方案 B**：给简化版本单独命名
```cpp
// 基础模板
template<typename K, typename V, typename KVData, int MAX_COLLISION = 4>
class ValueHashMapBase
{
    // ...
};

// 简化版本
template<typename K, typename V, int MAX_COLLISION = 4>
class ValueHashMap : public ValueHashMapBase<K, V, KeyValue<K, V>, MAX_COLLISION>
{
    // ...
};
```

**推荐**：采用方案 A（保持现有 HashMap.h 的设计模式）

---

## 🔍 步骤五：全项目搜索引用

### 5.1 使用 IDE 搜索功能

在 VS Code 中：
1. 按 `Ctrl+Shift+F` 打开全局搜索
2. 搜索：`HashMap<` 和 `ObjectHashMap<`
3. 检查返回的每个结果

### 5.2 关键搜索词

```
HashMapTemplate
HashMap<
ObjectHashMapTemplate
ObjectHashMap<
```

### 5.3 预期文件列表

- `HashMap.h` - 主定义（已改）
- 可能的单元测试文件
- 可能的示例代码文件
- 任何包含的其他头文件

### 5.4 针对每个引用的操作

对每个搜索结果：
1. ✅ 检查是否在 HashMap.h 中（已改）
2. ✅ 如果在其他文件，检查是否需要改动
3. ✅ 更新类型声明和使用处

---

## ✅ 步骤六：单元测试验证

### 6.1 运行现有测试

```bash
# 如果有 HashMap 的单元测试，运行它们
# 路径：通常在 /test 或 /Test 目录下
```

### 6.2 验证清单

- [ ] 编译无错误
- [ ] 编译无警告（或只有预期的警告）
- [ ] 所有 HashMap 相关单元测试通过
- [ ] 没有链接错误
- [ ] 可执行文件正常运行

### 6.3 简单的验证代码示例

```cpp
// 验证：值存储哈希映射可以正常使用
hgl::ValueHashMap<int, std::string> map;
map.Add(1, "one");
auto id = map.Find(1);
assert(id != -1);

// 验证：托管哈希映射可以正常使用
hgl::ManagedHashMap<int, MyClass*> obj_map;
MyClass* obj = new MyClass();
obj_map.Add(1, obj);
auto id2 = obj_map.Find(1);
assert(id2 != -1);

// 清理
// obj_map 析构时应该自动 delete obj
```

---

## 📚 步骤七：文档更新

### 7.1 更新注释

在 HashMap.h 中检查和更新所有注释，确保提及新名称：
```cpp
/**
 * ValueHashMap: 基于哈希的键值对容器，按值存储（O(1) 平均查找）
 * ...
 */
template<typename K, typename V, typename KVData>
class ValueHashMap
{
    // ...
};
```

### 7.2 更新项目文档

- [ ] 更新 README 或开发指南
- [ ] 更新 API 文档（如有）
- [ ] 更新代码示例

### 7.3 记录变更

在项目的 CHANGELOG 或版本说明中添加：
```
## [版本号] - 2026-01-24

### Changed
- Renamed `HashMap<K,V>` to `ValueHashMap<K,V>` for naming consistency
- Renamed `ObjectHashMap<K,V>` to `ManagedHashMap<K,V>` for naming consistency
- This change aligns HashMap classes with the existing naming convention:
  Value* = value-based storage, Managed* = managed pointer storage
```

---

## 🚀 步骤八：代码审查（如适用）

如果这是 pull request 工作流：

1. **创建分支**
   ```bash
   git checkout -b feature/rename-hashmap-classes
   ```

2. **提交改动**
   ```bash
   git add HashMap.h
   git commit -m "Rename HashMap classes for naming consistency"
   ```

3. **提交审查**
   - 在 PR 中清晰说明改名理由
   - 附上改名对照表
   - 说明编译和测试已通过

---

## ⚡ 快速检查清单

在提交前，确认以下项目：

- [ ] HashMap.h 中的所有类名已改
- [ ] 所有类型别名已更新
- [ ] 模板特化已更新
- [ ] 编译通过（无错误）
- [ ] 编译无相关警告
- [ ] 所有单元测试通过
- [ ] 项目其他文件无引用问题
- [ ] 注释和文档已更新
- [ ] 向团队沟通了改动

---

## 📞 常见问题

### Q1：是否会影响现有代码的兼容性？

**A**：是的，会影响。任何使用 `HashMap<K,V>` 或 `ObjectHashMap<K,V>` 的代码都需要改为新名称。

**解决方案**：
- 如果有旧代码，可以添加 using 别名保持兼容：
  ```cpp
  template<typename K, typename V>
  using HashMap = ValueHashMap<K, V>;  // 向后兼容
  ```

### Q2：如果改名后出现编译错误怎么办？

**A**：检查：
1. 是否有地方忘记改名
2. 继承关系是否正确
3. 模板参数是否对应
4. 特化版本是否都改了

### Q3：能否通过 typedef 实现向后兼容？

**A**：可以，但不推荐。新代码应该使用新名称。如需兼容，可以在头文件末尾添加：
```cpp
// 向后兼容（不推荐）
template<typename K, typename V, int MAX_COLLISION = 4>
using HashMap = ValueHashMap<K, V, MAX_COLLISION>;

template<typename K, typename V, int MAX_COLLISION = 4>
using ObjectHashMap = ManagedHashMap<K, V, MAX_COLLISION>;
```

---

## 总结

| 阶段 | 操作 | 预期时间 |
|------|------|---------|
| 分析定位 | 搜索引用，确定改动范围 | 30分钟 |
| 代码改动 | 修改类名、别名、特化版本 | 30分钟 |
| 编译验证 | 编译确保无错误 | 20分钟 |
| 测试验证 | 运行单元测试 | 30分钟 |
| 文档更新 | 更新注释和文档 | 20分钟 |
| **总计** | | **2-2.5小时** |

---

**祝改动顺利！** ✨
