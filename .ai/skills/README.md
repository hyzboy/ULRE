# 🚀 怎样在VS Code中使用ECS Render System Skills

## ✅ 配置已完成

已在 `.vscode/settings.json` 中自动启用了AI Skills功能。

## 📖 3种使用方式

### 方式1️⃣：询问Chat（推荐）

在VS Code中打开 **Github Copilot Chat**（或类似AI对话功能）：

```
@skills 我想添加一个SkySphere渲染系统
@skills 如何为Particle系统选择ExecutionPhase？
@skills 创建一个质量预设的RenderGraph
```

Chat会自动检索`.ai/skills`中的相关文档并提供针对性的建议。

---

### 方式2️⃣：直接打开文档

在VS Code中打开`.ai/skills`目录，浏览SKILL文档：

```
.ai/
└── skills/
    ├── SKILL_INDEX.md ⭐ 从这里开始
    ├── SKILL_QUICK_REFERENCE.md (快速查找)
    ├── SKILL_ADD_NEW_RENDER_COMPONENT.md (详细步骤)
    ├── SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md (原理)
    ├── SKILL_EXECUTION_PHASE_ORDERING.md (设计)
    ├── SKILL_RENDERGRAPH_USAGE.md (高级)
    └── manifest.json (自动索引)
```

**快速打开：**
- `Ctrl+K Ctrl+O` → 选择 `.ai/skills/SKILL_INDEX.md`
- 或在文件浏览器中右键 → Open in Editor

---

### 方式3️⃣：通过Markdown Preview

打开任意SKILL文档后：
- `Ctrl+Shift+V` 预览Markdown
- 点击链接快速跳转到其他SKILL
- 所有SKILL文档相互交叉引用

---

## 🎯 快速开始场景

### 场景A：我是新手，第一次添加系统

```
1. 打开 .ai/skills/SKILL_INDEX.md
2. 按"初学者路线"学习（2小时）
3. 复制 SKILL_QUICK_REFERENCE.md 中的Checklist
4. 按Checklist一步步实现
```

### 场景B：我知道要做什么，就需要代码模板

```
1. 打开 .ai/skills/SKILL_QUICK_REFERENCE.md
2. 找到"代码模板速查"部分
3. 复制对应的模板（最小Component、最小System等）
4. 填充自己的逻辑
```

### 场景C：我需要理解系统分组机制

```
1. 打开 .ai/skills/SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md
2. 阅读"核心概念"部分
3. 查看"实战场景"获得灵感
4. 在Chat中询问具体问题
```

### 场景D：我要创建复杂的RenderGraph

```
1. 打开 .ai/skills/SKILL_RENDERGRAPH_USAGE.md
2. 查看"自定义RenderGraph"部分
3. 复制最接近的示例代码
4. 调整参数以适应需求
```

---

## 💡 使用技巧

### ✨ Tip1：交叉引用导航
所有SKILL文档相互引用。打开一个文档后，可以快速跳转到相关主题。

### ✨ Tip2：Search in Markdown
- `Ctrl+Shift+F` 全文搜索`.ai`目录
- 搜索关键词如 "SkySphere", "Particle", "质量预设"

### ✨ Tip3：Markdown Table of Contents
每个SKILL文档顶部都有目录。使用VS Code的Outline视图：
- `Ctrl+Shift+O` 打开大纲
- 快速跳转到章节

### ✨ Tip4：并排编辑
在添加代码时，并排打开相关SKILL：
```
[SKILL_QUICK_REFERENCE.md] | [MySystem.cpp]
  (参考模板)                  (编写代码)
```

---

## 📍 文件位置参考

```
工作目录：e:\ULRE

Skills位置：
👉 e:\ULRE\.ai\skills\

配置文件：
👉 e:\ULRE\.vscode\settings.json (已自动配置)

文档副本（参考）：
👉 e:\ULRE\doc\SKILL_*.md
```

---

## 🔧 如果配置未生效

### 问题1：Chat中看不到Skills

**解决：**
1. 重启VS Code
2. 检查是否安装了GitHub Copilot或其他AI扩展
3. 确认 `.vscode/settings.json` 中 `"copilot.enable": true`

### 问题2：文件搜索找不到Skills

**解决：**
1. 检查 `.ai/skills/` 目录确实存在6个`.md`文件
2. 使用 `Ctrl+P` 然后输入 `SKILL_` 直接打开
3. 或手动打开文件浏览器定位

### 问题3：Markdown链接不工作

**解决：**
1. 在Markdown预览中（`Ctrl+Shift+V`），链接应该能工作
2. 如果仍未工作，确保文件名拼写完全一致

---

## 🎓 推荐学习顺序

### 第一周

📖 Monday: 
- 打开 `SKILL_INDEX.md`（3分钟理解结构）
- 打开 `SKILL_QUICK_REFERENCE.md`（5分钟速览）

📖 Tuesday-Wednesday:
- 完成第一个Component实现（用Checklist）
- 边做边参考 `SKILL_ADD_NEW_RENDER_COMPONENT.md`

📖 Thursday:
- 学习 `SKILL_SYSTEM_GROUPING_AND_ENABLEMENT.md`（15分钟）
- 尝试按元素类型启用/禁用系统

📖 Friday:
- 学习 `SKILL_EXECUTION_PHASE_ORDERING.md`（10分钟）
- 理解系统执行顺序的设计

### 第二周+

- 学习 `SKILL_RENDERGRAPH_USAGE.md`（15分钟）
- 尝试创建质量预设
- 实现多系统元素（如Particle）

---

## 💬 Chat提示词示例

### 添加新组件
```
@skills 
我想添加一个Terrain系统。
请根据SKILL告诉我：
1. 需要几个System？
2. 应该选什么ExecutionPhase？
3. 给出代码模板
```

### 调试问题
```
@skills
系统没有执行。我已经：
- 调用了SetRenderElementType("MyElement")
- 在DefaultSystemsCP中注册了系统
- 验证SceneStats检测到了Component

问题可能在哪里？查看SKILL_QUICK_REFERENCE中的常见错误
```

### 理解概念
```
@skills
我不理解SetElementTypeSystemsEnabled是怎么工作的。
请参考SKILL_SYSTEM_GROUPING_AND_ENABLEMENT
用简单的例子解释
```

---

## 🎁 你现在拥有

✅ 6个完整的SKILL文档  
✅ 中心导航Hub（SKILL_INDEX.md）  
✅ 代码模板库  
✅ Checklist和快速参考  
✅ 4个学习路径  
✅ manifest.json用于自动索引  
✅ 与Chat集成的AI知识库  

---

## 🚀 立即开始

**打开这个命令：**
```
Ctrl+K Ctrl+O  (或Ctrl+P)
输入: SKILL_INDEX
选择文件打开
```

或直接在Chat中问：
```
@skills 我想添加一个新的渲染系统，步骤是什么？
```

祝你的开发顺利！🎉
