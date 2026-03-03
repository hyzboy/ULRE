现代固定管线 Shader 生成器（现行基线，2026-03）

> 本文档是当前代码基线的实现约束与开发规范。

## 1. 目标与范围

当前渲染体系继续坚持“固定语义 + 可控变体”的路线：

- 上层按固定材质语义开发（BaseColor/Normal/Roughness/Metallic/AO 等）
- 下层通过模板与排列键生成不同路径变体
- 变体组合由平台与质量档约束，禁止业务侧自由扩张

本文件只描述“当前可用且受支持”的实现。

## 2. 当前实现基线（已落地）

### 2.1 材质创建路径

- 唯一受支持入口：`MaterialPreset + CreateConfig`
- `MaterialLibrary::CreateMaterialCreateInfo(MaterialPreset, cfg)` 已是直分发（switch -> CreateXXX）
- `MaterialManager` 仅保留 MaterialPreset / Material* 路径

### 2.2 运行边界（当前约束）

- 仅允许 `MaterialPreset + CreateConfig` 路径
- 不允许引入额外材质创建入口
- 不允许引入运行时按名称分发或文件解析分发

### 2.3 配置与语义分层

- MaterialInstance：仅参数数据
- Business Logic：业务语义（固定白名单）
- Composition / Template：按排列键组装 Shader

## 3. 开发强约束（必须遵守）

1. 新增内置材质必须走以下流程：
   - 在 `MaterialPreset` 增加枚举项
   - 在对应 CreateConfig 头声明 `CreateXxx`
   - 在 `MaterialLibrary.cpp` 的 MaterialPreset 分发中接入 `CreateXxx`

2. 禁止新增以下形式：
   - 字符串材质名创建
   - 运行时工厂注册/按名查找
   - 从材质文件解析生成运行时材质

3. 若新增变体轴，必须同时提供：
   - 轴值定义
   - 平台白名单与质量档约束
   - 失败回退策略

## 4. 变体与质量档策略

当前仍采用“轴驱动 + 档位裁剪”：

- 质量档：Low / Medium / High
- 平台裁剪优先，禁止未审核组合进入编译路径
- 变体键最小集保持稳定（渲染路径、输入集、光照模型、环境模型）

详细组合建议见：
- [ModernFixedRenderPipeline_VariantMatrix.md](ModernFixedRenderPipeline_VariantMatrix.md)

## 5. 编译与缓存要求

- 允许启动预编 + 运行时懒编译混合策略
- 懒编失败必须有回退（低档或默认材质）
- 缓存键必须覆盖：材质类型、渲染路径、输入集、光照模型、环境模型、平台宏版本

## 6. 验收标准（当前）

### 6.1 正确性

- 同材质在不同路径下语义一致（允许可解释差异）
- 环境模型切换无黑材质/崩溃

### 6.2 工程性

- Shader 生成失败有可定位诊断
- 编译失败可自动回退到可运行路径

### 6.3 约束一致性

- 代码与 CMake 层仅包含现行路径实现

## 7. 后续建议

- 在 CI 增加规则，阻止新增非基线材质路径
- 规范文档保持与代码同批更新，避免口径漂移

## 8. 结论

当前基线保持单一路径与可控变体：

- 稳定性由固定接口和白名单模板保证
- 灵活性由受控排列键与配置驱动保证

后续开发以本文为准执行。


