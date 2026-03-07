# 材质体系终极目标评估报告

## 1. 目标定义

目标：运行时不再执行 ShaderGen/GLSL 编译，所有材质变种在构建期离线生成并打包为 SPV，运行时仅做“按键查找 + 创建 VkShaderModule + 创建 Pipeline”。

你描述的目标在当前工程里是可达成的，而且仓库里已有可复用基础（`run_materialpreset_exhaustive.ps1` 与 `shadergen_materialpreset_dump/`）。

## 2. 当前体系现状（基于代码）

## 2.1 输入来源是“预设 + 配置”，不是用户任意动态脚本

- 预设枚举：`inc/hgl/mtl/MaterialLibrary.h` (`MaterialPreset`)
- 工厂分发：`src/ShaderGen/MaterialLibrary.cpp` (`CreateMaterialCreateInfo`)
- 运行时入口：`src/SceneGraph/module/MaterialManager.cpp` 的 `CreateMaterial(MaterialPreset, cfg)`

结论：主路径是内置预设，不是运行时用户输入 GLSL 文本。

## 2.2 变种轴已经存在（且可枚举）

- 基础配置轴：`Material2DCreateConfig` / `Material3DCreateConfig`
  - `prim`, `coordinate_system`, `camera`, `sky`, `local_to_world`, `sky_ambient_model`, `position_format`
- 光照排列轴：`ShaderPermutationKey`
  - `ambient`, `light`, `specular`, `shadow`
  - 文件：`inc/hgl/mtl/ShaderPermutationKey.h`
- 当前缓存键：`presetName + cfg->ToHashStdString()`
  - 文件：`src/SceneGraph/module/MaterialManager.cpp`

结论：已有“变种维度 + 哈希键”雏形，但还不是稳定的离线资产 ID 体系。

## 2.3 当前仍有运行时 ShaderGen/编译依赖

- `CreateMaterialCreateInfo(...)` 进入 `CompileComposedBusinessMaterial(...)`
- 再进入 `CompileFixedMaterial(...)`
- `CompileFixedMaterial` 内调用 `mci->CreateShader()` 执行 GLSL -> SPV
  - 文件：`src/ShaderGen/MaterialCompiler.cpp`

结论：今天运行时仍会触发 ShaderGen 与编译器插件（GLSLCompiler）。

## 2.4 你想要的方向已有验证样例

- 离线枚举脚本：`run_materialpreset_exhaustive.ps1`
- 产物目录：`shadergen_materialpreset_dump/`
- 产物索引：`shadergen_materialpreset_dump/spv_manifest.csv`

结论：已有“离线把预设变种导出 SPV”的实证路径，可直接进化为正式资产流水线。

## 3. 关于你提到的 Solid/Alpha/Mask/Dither

你提出的轴是合理的目标轴，但在当前主代码里，显式且稳定的输入轴主要还是上文的 `cfg + permutation key`。

- `ShaderComposition` 中确实有覆盖模式抽象（如 `PipelineCoverageMode`）
- 但当前预设工厂（如 `M_BasicLit.cpp`）多数直接固定输出模式（例如 `SingleRTAlphaBlend`）
- 说明“覆盖模式轴（Solid/Alpha/Mask）”尚未完全上升为统一、可枚举、可落盘的顶层输入协议

结论：如果终极目标要求完整覆盖 `Solid/Alpha/Mask/DitherMask`，需要把这些轴正式提升为“材质变种键”的一部分。

## 4. 可行性判断

结论：可行，且不需要推翻全部材质体系。

- 不需要重写底层 Vulkan 资源创建。
- 需要重构“材质上层生成契约”：从“运行时生成 `MaterialCreateInfo + 编译`”转为“运行时按 key 加载离线编译结果”。

## 5. 达成目标所需改造（核心清单）

## 5.1 建立稳定的 `MaterialVariantKey`（必须）

新增统一键结构，至少包含：

- `preset`
- `prim`
- `coordinate_system`(2D)
- `camera/sky/l2w`
- `ambient/light/specular/shadow`
- `coverage`(solid/alpha/mask/depth-only)
- `dither_mask`(on/off + 参数档位)
- `profile tier`（pc_high/mobile_low 等）

要求：

- key 序列化稳定（跨版本可控）
- key 可直接映射到 manifest 中唯一条目

## 5.2 设计离线产物协议（必须）

建议产物拆分：

- `manifest.json/csv`：key -> 文件路径 + 元数据
- `spv blobs`：按 stage 存储（vs/fs/gs...）
- `layout metadata`：descriptor layout、vertex layout、push constant、mi struct bytes
- `diagnostics`：可选，保留 mirror diff 与构建日志

要求：运行时不依赖 GLSL 文本即可完成 `MaterialCreateInfo` 重建或等价结构填充。

## 5.3 把运行时入口改为“加载优先，生成兜底”再逐步去兜底（必须）

建议分两步：

- 阶段 A：`LoadPrecompiledMaterialVariant(key)` 优先，miss 时走旧 ShaderGen（便于迁移）
- 阶段 B：发布配置禁用 runtime ShaderGen，miss 直接报错

对应改造点：

- `MaterialManager::CreateMaterial(MaterialPreset, cfg)`
- `CreateMaterialCreateInfo(...)` 的调用关系
- `CompileComposedBusinessMaterial(...)` 从 runtime 主路径移出

## 5.4 把 coverage/dither 轴上升到统一输入层（高优先）

如果你的产品目标明确包含 `Solid/Alpha/Mask + DitherMask`，必须：

- 在 `MaterialCreateConfig` 或并列扩展结构中显式建模
- 参与 `MaterialVariantKey` 和离线枚举
- 对应到 pipeline blend/depth/stencil 与 shader 宏/分支

否则离线产物会出现“逻辑上需要但键空间不可表达”的缺口。

## 5.5 设备能力分层策略（必须）

当前系统已有 `PhysicalDeviceProfileLite` 参与编译。离线后必须定义：

- 目标 profile 集（例如 mobile_low / desktop_mid / desktop_high）
- 每个 profile 允许的特性与变种集合
- 运行时 profile 选择规则（含降级映射）

避免“离线只编了 high profile，低端设备运行时 miss”。

## 5.6 构建与 CI 流水线（必须）

在 CI 增加固定工序：

1. 变种枚举
2. SPV 生成
3. manifest 生成
4. 产物完整性检查（缺失 stage、重复 key、坏文件）
5. 运行时 dry-run（仅加载，不编译）

并把 `shadergen_materialpreset_dump` 脚本能力纳入正式 build target。

## 6. 是否需要“重新设计材质上层生成体系”

结论：需要“重设计上层接口契约”，但不是“推倒重来”。

不需要重做：

- 底层 Vulkan module/pipeline 创建
- 描述符与顶点输入的核心数据结构

需要重做/新增：

- 统一 VariantKey
- 预编译 manifest 协议
- Runtime 资产加载器
- 失败策略（strict no-runtime-compile）
- 版本化策略（key/schema/profile）

## 7. 推荐分期路线

## Phase 0: 对齐与冻结（1-2 周）

- 冻结首批目标预设（例如 16 个内置 preset）
- 冻结首批变种轴（至少 cfg + permutation，外加 coverage/dither 设计草案）
- 定义 VariantKey v1

## Phase 1: 离线导出产品化（2-4 周）

- 把现有 exhaustive 脚本改为标准工具命令
- 生成 manifest v1 + SPV 包
- 产物校验与报表落地

## Phase 2: 运行时加载路径接入（2-4 周）

- MaterialManager 增加 `LoadPrecompiled...` 主路径
- miss 时 fallback 到现有 runtime ShaderGen（开发期开关）
- 增加统计：命中率、miss 原因、回退次数

## Phase 3: 覆盖轴补齐（2-6 周）

- 把 `Solid/Alpha/Mask/DitherMask` 明确定义并纳入 key
- 补齐离线枚举、产物与运行时选择

## Phase 4: 去 runtime ShaderGen（1-2 周）

- 发布配置下禁用 GLSLCompiler 依赖
- miss 直接失败并给出清晰诊断
- 保留开发模式开关用于调试

## 8. 主要风险与对策

- 风险：变种爆炸导致包体过大
  - 对策：分层 profile、按场景分包、冷热分离加载

- 风险：键不稳定导致缓存/资产失配
  - 对策：VariantKey schema version + manifest version + CI 校验

- 风险：coverage/dither 未统一建模导致线上漏变种
  - 对策：先完成输入协议再宣布去 runtime compile

- 风险：平台特性差异（扩展、精度）
  - 对策：离线按 profile 编译，运行时做 profile 映射与降级

## 9. 验收标准（建议）

- 运行时主流程不调用 `CompileFixedMaterial/CompileComposedBusinessMaterial`
- 发布包不依赖 GLSLCompiler 动态库
- 关键场景材质创建 `SPV manifest hit rate >= 99.9%`
- 所有 miss 都能定位到缺失 key 或 profile 映射问题
- 冷启动材质创建耗时显著下降且稳定

## 10. 结论

你的终极目标在当前架构上可达成，且已有离线导出基础。真正需要的不是重写渲染器，而是把“运行时生成材质”升级为“离线产物驱动材质”。

最关键的成败点只有两个：

1. 统一且可版本化的 `MaterialVariantKey`
2. 严格执行的离线产物与运行时加载协议

只要这两点做实，去掉运行时 ShaderGen 是工程问题，不是技术不可达问题。
