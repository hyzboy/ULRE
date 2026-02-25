# 模板类命名改进建议 - 快速参考

## 📊 命名现状与建议对比

### **现有模板分类统计**
- ✅ **Value系列（按值存储）**：ValueBuffer, ValueArray, IndexedList, OrderedSet, ValueKVMap - **5个** 
- ✅ **Ptr系列（指针存储，用户管理）**：PtrArray - **1个**
- ✅ **Managed系列（指针+自动管理）**：ManagedArray, OrderedManagedSet - **2个**
- ⚠️ **HashMap系列（新增，命名不一致）**：HashMap, ObjectHashMap - **2个** ← **需要改进**
- ✅ **其他**：Map, ObjectMap, Pool, Queue, Stack 等 - **多个**

---

## 🔴 核心问题：HashMap 命名不一致

### 对比展示

```
现有规则:              HashMap系列:        推荐改为:
─────────────         ──────────────     ──────────────
ValueArray       →    HashMap        →   ValueHashMap
ManagedArray     →    ObjectHashMap  →   ManagedHashMap
ValueKVMap       →    (已遵循规则)      (需升级)
```

### 具体问题

| 方面 | 现状 | 问题 | 改进后 |
|-----|------|------|--------|
| **基础模板** | `HashMapTemplate` | Template后缀冗长 | `ValueHashMap<K,V,KVData>` |
| **简化版本** | `HashMap` | 缺少Value前缀 | `ValueHashMap<K,V>` |
| **对象版本基础** | `ObjectHashMapTemplate` | Object前缀模糊 | `ManagedHashMap<K,V,KVData>` |
| **对象简化版本** | `ObjectHashMap` | Object前缀不如Managed清晰 | `ManagedHashMap<K,V>` |
| **与其他容器一致性** | ❌ 不一致 | 前缀规则混乱 | ✅ 一致 |

---

## ✅ 完整模板系列展示

### **按值存储系列（Value）**
```cpp
ValueBuffer<T>          // 基础缓冲区
  └─ ValueArray<T>      // 动态数组
      ├─ ValueHashMap<K,V>     // ✨ 新：哈希映射（推荐改名）
      ├─ ValueKVMap<K,V>       // 紧凑K-V映射
      └─ Map<K,V>              // 有序K-V映射

IndexedList<T>    // 索引数组
OrderedSet<T>      // 有序集合
```

### **指针+托管系列（Managed）**
```cpp
ManagedArray<T>         // 动态数组
  ├─ ManagedHashMap<K,V>     // ✨ 新：哈希映射（推荐改名）
  └─ ObjectMap<K,V>          // 有序K-V映射

OrderedManagedSet<T>    // 有序集合
```

### **指针+用户管理系列（Ptr）**
```cpp
PtrArray<T>             // 动态数组
                        // （暂无其他容器）
```

### **专用系列**
```cpp
Pool<T>                 // 值类型对象池
ObjectPool<T>           // 指针类型对象池
Queue<T>                // 队列
Stack<T>                // 栈
ConstStringSet<SC>      // 常量字符串集合
String<T>               // 字符串
StringList<T>           // 字符串列表
```

---

## 📋 命名规则说明

### **前缀含义**
| 前缀 | 含义 | 示例 | 特点 |
|-----|------|------|------|
| `Value*` | 按值存储 | ValueArray, ValueHashMap | 仅支持平凡类型，高效 |
| `Ptr*` | 指针存储 | PtrArray | 用户管理生命周期 |
| `Managed*` | 指针+自动管理 | ManagedArray, ManagedHashMap | 支持非平凡类型，自动delete |
| `Indexed*` | 索引访问 | IndexedList | 数据位置固定，索引可调 |
| `Ordered*` | 有序去重 | OrderedSet | 自动排序，无重复 |

### **后缀含义**
| 后缀 | 含义 | 示例 | 访问方式 |
|-----|------|------|---------|
| `*Buffer` | 基础缓冲 | ValueBuffer | 连续 |
| `*Array` | 动态数组 | ValueArray | 连续 |
| `*Set` | 有序集合 | OrderedSet | 有序+去重 |
| `*Map` | 哈希映射 | ValueHashMap | O(1)平均 |
| `*Registry` | 注册表 | IDNameRegistry | ID+对象管理 |
| `*Pool` | 对象池 | ObjectPool | 对象复用 |

---

## 🎯 改进方案对比

### **方案A：前缀规则化（推荐 ⭐⭐⭐⭐⭐）**

```cpp
// HashMap系列改为：
HashMapTemplate<K,V,KVData>     → ValueHashMap<K,V,KVData>
HashMap<K,V>                    → ValueHashMap<K,V>
ObjectHashMapTemplate<K,V,KVData> → ManagedHashMap<K,V,KVData>
ObjectHashMap<K,V>              → ManagedHashMap<K,V>
```

**优势**：
- ✅ 与整个容器系统规则一致
- ✅ 避免 "Object" 模糊前缀
- ✅ 去掉 "Template" 冗长后缀
- ✅ 新开发者易于理解
- ✅ 易于扩展（如 PtrHashMap）

**不足**：
- ⚠️ 需要改名，涉及代码调整

---

### **方案B：保持现状**

```cpp
// 保持不变
HashMap<K,V>
ObjectHashMap<K,V>
```

**优势**：
- ✅ 零改动

**不足**：
- ❌ 与新增模板系列不一致
- ❌ "Object" 前缀语义不清
- ❌ 违反已建立的规则
- ❌ 未来扩展时会造成混乱

---

## 📊 改进影响分析

### **受影响范围**

| 项目 | 当前文件 | 涉及改动 | 复杂度 |
|-----|---------|---------|--------|
| 类名改写 | HashMap.h | 4个类名 | 低 |
| 类型别名 | HashMap.h | 2个using | 低 |
| 模板特化 | HashMap.h | 2个特化版本 | 中 |
| 其他文件引用 | 需扫描 | 待评估 | 中 |
| 注释更新 | 需更新 | 低 |
| 文档更新 | 本文档 | 低 |

### **工作量估算**

- **代码改动**：2-4小时（改名+编译验证）
- **测试验证**：1-2小时（单元测试）
- **文档更新**：1小时
- **总计**：4-7小时

---

## ✨ 改进后的完整系统

### **一览表**
```
┌─────────────────────────────────────────────────────┐
│           模板容器系统完整全景                          │
├─────────────────────────────────────────────────────┤
│ VALUE 系列（按值存储，平凡类型）                      │
│  ValueBuffer<T>                                     │
│  ValueArray<T>                                      │
│  IndexedList<T>                               │
│  OrderedSet<T>                                 │
│  ValueKVMap<K,V>                                    │
│  ✨ ValueHashMap<K,V>      ← 新命名               │
│                                                     │
│ PTR 系列（指针存储，用户管理）                        │
│  PtrArray<T>                                        │
│                                                     │
│ MANAGED 系列（指针+自动管理）                        │
│  ManagedArray<T>                                    │
│  OrderedManagedSet<T>                               │
│  ObjectMap<K,V>                                     │
│  ✨ ManagedHashMap<K,V>    ← 新命名               │
│                                                     │
│ 其他通用容器                                         │
│  Pool<T>, ObjectPool<T>                            │
│  Queue<T>, Stack<T>                                │
│  String<T>, StringList<T>                          │
│  ConstStringSet<SC>                                │
│                                                     │
└─────────────────────────────────────────────────────┘
```

---

## 🚀 建议行动方案

### **短期（立即）**
1. **审核** HashMap 系列设计实现 ✓ 已完成
2. **确认** 是否接受重命名建议
3. **制定** 改名计划

### **中期（这个周期）**
1. **实施** 类名重命名（ValueHashMap / ManagedHashMap）
2. **编译验证** 确保无编译错误
3. **运行测试** 验证功能完整性

### **长期（文档维护）**
1. **更新** 开发文档和注释
2. **更新** 代码示例
3. **建立** 命名规则指南，指导未来新增模板

---

## 📌 关键建议

### **必做项**
- [ ] 将 `HashMap<K,V>` 改为 `ValueHashMap<K,V>`
- [ ] 将 `ObjectHashMap<K,V>` 改为 `ManagedHashMap<K,V>`
- [ ] 更新所有依赖这些类名的代码

### **可选项**
- [ ] 将 `HashMapTemplate` 改为 `ValueHashMap<K,V,KVData>`（保持Template后缀也可接受）
- [ ] 将 `ObjectHashMapTemplate` 改为 `ManagedHashMap<K,V,KVData>`

### **文档项**
- [ ] 在项目文档中记录命名规则
- [ ] 为新开发者编写"容器选择指南"
- [ ] 建立模板命名规范

---

## 💡 总结

### 现状评价
- **总体**：命名体系已经很科学，新增HashMap系列设计合理
- **问题**：HashMap 命名与已有规则不完全一致
- **影响**：轻微（仅4个类名），改动收益高

### 改进后的好处
- ✅ 规则完全统一
- ✅ 系统更易理解和维护  
- ✅ 为未来扩展奠定基础
- ✅ 新开发者学习成本降低

### 最终建议
**强烈推荐实施方案A**，改进HashMap系列的命名，使其与整个容器系统规则保持一致。

---

**文档版本**：1.0  
**更新日期**：2026-01-24  
**优先级**：高（建议立即改进）
