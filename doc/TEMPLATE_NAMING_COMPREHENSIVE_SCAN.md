# 模板类命名综合扫描报告与建议

**报告日期**：2026年1月24日  
**状态**：完整的模板命名审查与优化建议

---

## 一、现状概览

### 1. 现有模板类清单（按分类）

#### **基础缓冲层（Value系列 - 按值存储，平凡类型）**
| 序号 | 当前名称 | 用途 | 存储方式 | 访问方式 |
|-----|---------|------|---------|---------|
| 1 | `ValueBuffer<T>` | ✅ 轻量级缓冲区，基础实现 | 值存储 | 连续 |
| 2 | `ValueArray<T>` | ✅ 动态数组 | 值存储 | 连续 |
| 3 | `IndexedList<T>` | ✅ 索引访问数组 | 值存储 | 索引 |
| 4 | `OrderedSet<T>` | ✅ 有序去重集合 | 值存储 | 有序 |
| 5 | `ValueKVMap<K,V>` | ✅ 键值对映射 | 值存储 | 有序K-V |

#### **指针存储层（Ptr系列 - 按指针存储，用户管理）**
| 序号 | 当前名称 | 用途 | 存储方式 | 访问方式 |
|-----|---------|------|---------|---------|
| 6 | `PtrArray<T>` | ✅ 非平凡类型指针数组 | 指针 | 连续 |

#### **托管存储层（Managed系列 - 按指针存储，自动管理生命周期）**
| 序号 | 当前名称 | 用途 | 存储方式 | 访问方式 |
|-----|---------|------|---------|---------|
| 7 | `ManagedArray<T>` | ✅ 自动管理非平凡类型数组 | 指针 | 连续 |
| 8 | `OrderedManagedSet<T>` | ✅ 自动管理有序集合 | 指针 | 有序 |

#### **HashMap新增家族（哈希映射）**
| 序号 | 当前名称 | 用途 | 新增 | 位置 |
|-----|---------|------|------|------|
| 9 | `HashMapTemplate<K,V,KVData>` | 哈希映射基础模板 | ✅ 新增 | HashMap.h |
| 10 | `HashMap<K,V>` | 按值存储的哈希映射 | ✅ 新增 | HashMap.h |
| 11 | `ObjectHashMapTemplate<K,V,KVData>` | 对象哈希映射基础模板 | ✅ 新增 | HashMap.h |
| 12 | `ObjectHashMap<K,V>` | 按指针存储的哈希映射 | ✅ 新增 | HashMap.h |

#### **其他重要模板**
| 序号 | 当前名称 | 用途 |
|-----|---------|------|
| 13 | `Map<K,V>` | 基于二叉搜索的有序映射 |
| 14 | `ObjectMap<K,V>` | 对象版本的有序映射 |
| 15 | `Pool<T>` | 对象池 |
| 16 | `ObjectPool<T>` | 对象指针池 |
| 17 | `Queue<T>` | 队列 |
| 18 | `Stack<T>` | 栈 |
| 19 | `ConstStringSet<SC>` | 常量字符串集合 |
| 20 | `IDName<SC>` | ID-名称注册表 |
| 21 | `String<T>` | 字符串 |
| 22 | `StringList<T>` | 字符串列表 |

---

## 二、现有命名方案评估

### 优势
✅ **前缀清晰**：Value/Ptr/Managed前缀明确表达存储和管理方式  
✅ **后缀一致**：Array/Set/Map后缀表达访问特性  
✅ **层次清晰**：基础层→高层级的继承关系清晰  
✅ **新增命名遵循规则**：HashMap系列遵循已有规则  

### 问题与改进空间
⚠️ **HashMap命名不够清晰**：
- `HashMapTemplate` 的 Template 后缀显得冗长
- 应该采用更简洁的命名方式

⚠️ **ObjectHashMap 前缀不一致**：
- 与 `ManagedArray`, `OrderedManagedSet` 使用 `Managed` 前缀不一致
- 应该改用 `ManagedHashMap` 保持一致性

---

## 三、科学合理的新命名方案

### 方案规则（已验证与现有命名一致）

#### **规则1：前缀区分存储和管理方式**
```
Value*        → 按值存储（平凡类型）+ 轻量实现
Ptr*          → 按指针存储，用户自己管理生命周期（不拥有）
Managed*      → 按指针存储，容器自动管理生命周期（拥有）
```

#### **规则2：后缀区分访问方式和功能**
```
*Buffer       → 基础缓冲区实现（最底层）
*Array        → 连续访问动态数组
*Set          → 有序去重集合
*Map          → 键值对映射
*List         → 列表（特殊用途，如字符串列表）
*Registry     → 注册表（带ID管理）
*Pool         → 对象池
*View         → 视图（字符串视图）
```

#### **规则3：命名原则（优先级排序）**
1. **准确性**：名称必须准确表达容器的本质特性
2. **一致性**：同一系列遵循相同的前缀/后缀规则
3. **简洁性**：避免冗长或重复的词汇
4. **易读性**：优先使用常见的术语

---

## 四、完整命名建议表

### 第一组：基础缓冲层

| 序号 | 当前名称 | 建议新名称 | 理由 | 优先级 |
|-----|---------|----------|------|--------|
| 1 | `ValueBuffer<T>` | **保持** | 名称已经很优化 | ✅ 保留 |

### 第二组：连续访问数组（Value系列）

| 序号 | 当前名称 | 建议新名称 | 理由 | 优先级 |
|-----|---------|----------|------|--------|
| 2 | `ValueArray<T>` | **保持** | 简洁清晰，符合规则 | ✅ 保留 |
| 6 | `PtrArray<T>` | **保持** | 简洁清晰，符合规则 | ✅ 保留 |
| 7 | `ManagedArray<T>` | **保持** | 与OrderedManagedSet一致 | ✅ 保留 |

### 第三组：索引访问数组

| 序号 | 当前名称 | 建议新名称 | 理由 | 优先级 |
|-----|---------|----------|------|--------|
| 3 | `IndexedList<T>` | **保持** | 准确表达"索引"+"按值存储" | ✅ 保留 |

### 第四组：有序去重集合

| 序号 | 当前名称 | 建议新名称 | 理由 | 优先级 |
|-----|---------|----------|------|--------|
| 4 | `OrderedSet<T>` | **保持** | "有序"+"去重"+"按值存储" | ✅ 保留 |
| 8 | `OrderedManagedSet<T>` | **保持** | 与OrderedValueSet系列一致 | ✅ 保留 |

### 第五组：键值对映射

| 序号 | 当前名称 | 建议新名称 | 理由 | 优先级 |
|-----|---------|----------|------|--------|
| 5 | `ValueKVMap<K,V>` | **保持** | 明确表达"按值"+"KV" | ✅ 保留 |
| 13 | `Map<K,V>` | **保持** | 简化版本，继承自MapTemplate | ✅ 保留 |

### 第六组：哈希映射（新增 - **重点优化**）

| 序号 | 当前名称 | 建议新名称 | 理由 | 优先级 |
|-----|---------|----------|------|--------|
| 9 | `HashMapTemplate<K,V,KVData>` | **改为** `ValueHashMap<K,V,KVData>` | 与规则一致；明确"值存储"的哈希映射基础模板 | 🔴 **强烈推荐** |
| 10 | `HashMap<K,V>` | **改为** `ValueHashMap<K,V>` | 简化名称，遵循规则，对应Map | 🔴 **强烈推荐** |
| 11 | `ObjectHashMapTemplate<K,V,KVData>` | **改为** `ManagedHashMap<K,V,KVData>` | 与ManagedArray一致；明确"指针+自动管理" | 🔴 **强烈推荐** |
| 12 | `ObjectHashMap<K,V>` | **改为** `ManagedHashMap<K,V>` | 简化名称，遵循规则，对应ObjectMap | 🔴 **强烈推荐** |

### 第七组：对象池

| 序号 | 当前名称 | 建议新名称 | 理由 | 优先级 |
|-----|---------|----------|------|--------|
| 15 | `Pool<T>` | **保持** | 简洁，对应ValueArray | ✅ 保留 |
| 16 | `ObjectPool<T>` | **保持** | 对应ManagedArray | ✅ 保留 |

### 第八组：其他容器（保持不变）

| 序号 | 当前名称 | 建议 | 理由 |
|-----|---------|------|------|
| 17 | `Queue<T>` | 保持 | 标准命名 |
| 18 | `Stack<T>` | 保持 | 标准命名 |
| 19 | `ConstStringSet<SC>` | 保持 | 特殊用途，命名恰当 |
| 20 | `IDNameRegistry<M,SC>` | 保持或改为 `IDNameSet<M,SC>` | 如认为"Registry"过长可改为"Set" |
| 21 | `String<T>` | 保持 | 标准命名 |
| 22 | `StringList<T>` | 保持 | 特殊用途，命名恰当 |

---

## 五、HashMap系列命名改进详解

### 当前存在的问题

1. **"Object"前缀在新规则下不清晰**
   - 旧规则：Object = 指针类型
   - 新规则：Managed = 拥有、自动管理生命周期
   - **冲突**：ObjectHashMap 中的 "Object" 不够精确

2. **"Template"后缀显得冗长**
   - `HashMapTemplate` 对比 `Map`（直接用）
   - 应该遵循 `MapTemplate` 和 `ObjectMapTemplate` 的命名模式的升级版本

### 改进方案对比

#### 方案A：采用前缀+后缀规则（**推荐**）
```cpp
// 值存储哈希映射
HashMapTemplate<K,V,KVData> → ValueHashMap<K,V,KVData>
HashMap<K,V>                → ValueHashMap<K,V>

// 指针+自动管理哈希映射
ObjectHashMapTemplate<K,V,KVData> → ManagedHashMap<K,V,KVData>
ObjectHashMap<K,V>                → ManagedHashMap<K,V>
```

**优势**：
- ✅ 与 `ValueArray`, `ManagedArray`, `ValueKVMap` 等保持一致
- ✅ 遵循"Value/Managed + 功能"规则
- ✅ 去掉了冗长的 "Object" 前缀
- ✅ 明确表达存储和管理方式

#### 方案B：保持现状
```cpp
// 现状保留
HashMap<K,V>
ObjectHashMap<K,V>
```

**问题**：
- ⚠️ 与新增的模板系列（ValueArray, ManagedArray）前缀不一致
- ⚠️ "ObjectHashMap" 的语义不如 "ManagedHashMap" 清晰
- ⚠️ 与后续可能的Ptr哈希映射（`PtrHashMap`）不协调

---

## 六、完整系列对应关系

### **Series 1: 基础+连续访问**
```
ValueBuffer<T>
  ├─ ValueArray<T>        （在ValueBuffer基础上扩展）
  ├─ ValueHashMap<K,V>    （哈希版本，按值）
  ├─ ValueKVMap<K,V>      （紧凑K-V版本，按值）
  └─ Map<K,V>             （有序K-V版本，按值）

ManagedArray<T>           （对应 ValueArray 的托管版本）
  ├─ ManagedHashMap<K,V>  （哈希版本，托管）
  └─ ObjectMap<K,V>       （有序K-V版本，托管）
```

### **Series 2: 索引访问**
```
IndexedList<T>      （索引版本，按值）
```

### **Series 3: 有序集合**
```
OrderedSet<T>        （有序集合，按值）
OrderedManagedSet<T>      （有序集合，托管）
```

---

## 七、实施步骤与工作量评估

### **优先级分级**

#### 🔴 **第一阶段（强烈推荐，核心改进）**
需要改进的项目（新增HashMap系列）：
- [ ] `HashMapTemplate` → `ValueHashMap<K,V,KVData>`
- [ ] `HashMap` → `ValueHashMap<K,V>`
- [ ] `ObjectHashMapTemplate` → `ManagedHashMap<K,V,KVData>`
- [ ] `ObjectHashMap` → `ManagedHashMap<K,V>`

**影响范围**：HashMap.h 文件，引用较少
**工作量**：低（主要是类名重命名）

#### ✅ **第二阶段（可选，长期优化）**
- [ ] `IDNameRegistry` 可考虑改为 `IDNameSet`（如认为Registry过长）

**影响范围**：IDName.h 文件
**工作量**：中等（涉及多处调用）

#### ✅ **第三阶段（保持现状）**
所有其他现有模板保持不变，已经符合命名规则：
- ValueBuffer, ValueArray, IndexedList
- PtrArray, ManagedArray
- OrderedSet, OrderedManagedSet
- ValueKVMap, Map, ObjectMap
- Pool, ObjectPool
- Queue, Stack
- ConstStringSet, String, StringList, etc.

---

## 八、建议总结

### **核心建议**

**1. HashMap系列必须优化**（优先级：高）
- 当前的 `HashMap` / `ObjectHashMap` 命名与新增模板系列不一致
- 应改为 `ValueHashMap` / `ManagedHashMap`，与其他容器前缀规则保持一致
- 同时移除冗长的 "Template" 后缀，保持简洁

**2. 现有模板命名已经很合理**（优先级：低）
- Value*/Ptr*/Managed* 前缀系统明确
- *Array/*Set/*Map/*Pool 后缀清晰
- 无需大规模调整

**3. 新增模板应严格遵循规则**（优先级：高）
- HashMap系列设计很好，但命名需要调整
- 未来新模板务必遵循"前缀+后缀"规则
- 避免 "Object" 这种模糊前缀

### **预期收益**

✅ **一致性**：所有容器遵循统一的命名规则  
✅ **可读性**：新开发者能快速理解容器的存储和管理特性  
✅ **维护性**：相关容器族群清晰，便于演进  
✅ **扩展性**：未来新增容器可以无缝融入系统  

---

## 九、后续验证清单

- [ ] 确认 HashMap 系列是否接受重命名
- [ ] 如接受，更新 HashMap.h 文件中的所有类名
- [ ] 更新所有依赖 HashMap 的代码文件
- [ ] 更新相关文档和注释
- [ ] 运行完整的编译和单元测试验证
- [ ] 更新本命名方案文档

---

**报告完成**  
建议人：AI 代码分析系统  
审核状态：待确认
