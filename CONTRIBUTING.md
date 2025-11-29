# 贡献指南 / Contributing Guide

感谢您对ULRE项目的关注！本文档将帮助您了解如何为项目做出贡献。

Thank you for your interest in contributing to ULRE! This document will help you understand how to contribute to the project.

## 开发环境设置 / Development Environment Setup

### 先决条件 / Prerequisites

- **编译器**: GCC 11+, Clang 13+, 或 MSVC 2019+
- **CMake**: 3.28 或更高版本
- **Vulkan SDK**: 1.3 或更高版本
- **vcpkg**: 用于依赖管理

### 快速开始 / Quick Start

1. **克隆仓库 / Clone the repository**
```bash
git clone --recursive https://github.com/hyzboy/ULRE.git
cd ULRE
```

2. **安装依赖 / Install dependencies**
```bash
# 使用 vcpkg
vcpkg install glm jsoncpp expat
```

3. **配置和构建 / Configure and build**
```bash
# 使用 CMake Presets
cmake --preset debug
cmake --build --preset debug
```

## 代码风格 / Code Style

### C++ 代码规范

- 使用 4 空格缩进
- 花括号使用 Allman 风格
- 类名使用 PascalCase
- 函数名使用 PascalCase
- 变量名使用 snake_case
- 常量使用 UPPER_CASE

### 运行代码格式化

```bash
# 格式化所有源文件
find src inc -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

## 提交规范 / Commit Guidelines

### 提交信息格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

### 类型 / Types

- `feat`: 新功能
- `fix`: Bug修复
- `docs`: 文档更新
- `style`: 代码格式化
- `refactor`: 重构
- `perf`: 性能优化
- `test`: 测试相关
- `chore`: 构建/工具变更

### 示例

```
feat(renderer): 添加多重采样抗锯齿支持

- 实现 MSAA 4x/8x 支持
- 更新 RenderTarget 配置接口
- 添加示例代码

Closes #123
```

## 提交Pull Request

1. Fork 仓库
2. 创建功能分支: `git checkout -b feature/my-feature`
3. 提交更改: `git commit -am 'feat: add my feature'`
4. 推送分支: `git push origin feature/my-feature`
5. 创建 Pull Request

## 报告问题 / Reporting Issues

使用 GitHub Issues 报告问题时，请包含：

- 操作系统和版本
- GPU 型号和驱动版本
- Vulkan SDK 版本
- 问题的详细描述
- 复现步骤
- 相关的日志输出

## 联系方式 / Contact

- GitHub Issues: 技术问题和功能请求
- Email: [项目维护者邮箱]

---

再次感谢您的贡献！

Thank you again for your contribution!
