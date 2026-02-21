# ECS Render System SKILL合集

本目录包含5个SKILL文档，涵盖添加新Component/System、系统分组、ExecutionPhase、RenderGraph和快速参考。

## 🎯 SKILL导航

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
1. 阅读 [SKILL_QUICK_REFERENCE.md](SKILL_QUICK_REFERENCE.md) - 5 min
2. 完成第一个元素（用Checklist）- 45 min
3. 阅读 [SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md](SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md) - 15 min
4. 尝试3个不同Element Type - 45 min

### 🟡 进阶路线（3小时）
1. 阅读所有5个SKILL - 60 min
2. 创建多系统元素（如Particle例子）- 60 min
3. 尝试自定义RenderGraph和质量预设 - 60 min

### 🔴 高级优化（额外）
- 性能分析（帧率下降时自动降级）
- 多RT延迟渲染
- 从配置文件加载质量预设

---

## 🔗 与项目其他文件的关系

```
doc/ 目录结构
├── SKILL_*.md (本集合，5个文件)
│   ├── SKILL_ADD_NEW_RENDER_COMPONENT.md
│   ├── SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md
│   ├── SKILL_EXECUTION_PHASE_ORDERING.md
│   ├── SKILL_RENDERGRAPH_USAGE.md
│   ├── SKILL_QUICK_REFERENCE.md
│   └── SKILL_INDEX.md (本文件)
│
├── RENDER_SYSTEM_SIMPLIFICATION_PLAN.md ← 背景/历史
│
└── 未来扩展 (计划)
    ├── SKILL_PARTICLE_SYSTEM_IMPL.md
    ├── SKILL_DECAL_SYSTEM_IMPL.md
    ├── SKILL_TERRAIN_RENDERING.md
    └── SKILL_DEFERRED_RENDERING.md

源码位置 (参考)
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
A: 如果你不知道，从 [SKILL_QUICK_REFERENCE.md](SKILL_QUICK_REFERENCE.md) 开始。

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
| SKILL_QUICK_REFERENCE.md | 📊 浅 | 5 min | ⭐ | ✓ |
| SKILL_ADD_NEW_RENDER_COMPONENT.md | 📘 深 | 20 min | ⭐⭐ | ✓✓ |
| SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md | 📗 中等 | 15 min | ⭐⭐ | ✓ |
| SKILL_EXECUTION_PHASE_ORDERING.md | 📙 中等 | 10 min | ⭐ | ✗ |
| SKILL_RENDERGRAPH_USAGE.md | 📕 中等 | 15 min | ⭐⭐ | ✓ |

---

## 🎯 关键主题快速链接

| 主题 | 相关SKILL | 快速用法 |
|------|---------|---------|
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

这5个SKILL文档提供了从零到精通的**完整路径**，支持：

- ✅ 快速上手（QUICK_REFERENCE）
- ✅ 详细学习（各topic SKILL）
- ✅ 代码复制（模板）
- ✅ 问题解决（常见错误、诊断）
- ✅ 深度理解（原理篇）
- ✅ 实践项目（示例场景）

**立即开始：** 打开 [SKILL_QUICK_REFERENCE.md](SKILL_QUICK_REFERENCE.md) 的Checklist部分。

祝你的新元素实现顺利！ 🚀
