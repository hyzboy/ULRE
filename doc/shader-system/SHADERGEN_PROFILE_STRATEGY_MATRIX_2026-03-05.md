# ShaderGen Profile 驱动策略矩阵（2026-03-05）

## 1. 范围

本文定义 `PhysicalDeviceProfileLite` 字段如何影响 ShaderGen 编译策略、校验规则与降级行为。

- 输入来源：
  - Runtime：`VulkanPhyDevice` 采样
  - JSON：collector 导入
- 生效入口：`GLSLCompiler` 的 profile 应用逻辑（`GetLimit/SetLimit`）
- 观测出口：validator diagnostics + strict gate category

---

## 2. 字段 -> 影响点 -> 行为

| Profile 字段 | 影响模块 | 行为 | 失败/降级输出 |
|---|---|---|---|
| `api_version` | `GLSLCompiler` | 选择 Vulkan/SPV 目标版本（1.0~1.3 / 1.0~1.6） | 编译日志记录目标版本 |
| `limits.max_vertex_input_attributes` | `GLSLCompiler` limits + Validator | 限制顶点属性相关 resource 上限 | profile 校验失败，归类 `StrictGate.Profile` |
| `limits.max_bound_descriptor_sets` | Validator | 校验 descriptor set 总量约束 | profile 校验失败，归类 `StrictGate.Profile` |
| `limits.max_uniform_buffer_range` | `GLSLCompiler` limits + Validator | 限制 UBO 相关组件预算与范围 | profile 校验失败，归类 `StrictGate.Profile` |
| `limits.max_storage_buffer_range` | `GLSLCompiler` limits + Validator | 限制 SSBO/atomic counter buffer 预算 | profile 校验失败，归类 `StrictGate.Profile` |
| `features.geometry_shader` | Validator + 编译阶段 | 几何着色能力开关（无能力时拒绝 GS 路径） | profile 校验失败，归类 `StrictGate.Profile` |
| `features.tessellation_shader` | `GLSLCompiler` limits | tessellation 相关 limit 置零（禁用） | 走降级/禁用路径 |
| `features.descriptor_indexing` | `GLSLCompiler` limits | indexing 相关 limits 开关 | 无该能力时不启用 indexing |

---

## 3. 策略原则

1. **单一限制入口**：只使用 `GetLimit/SetLimit` 写入编译限制。
2. **设备级一次性设置**：profile 在设备创建阶段设置一次，不在材质创建阶段重复切换。
3. **同 DTO 同行为**：runtime/json 进入同一个 profile DTO 与同一 apply 逻辑。
4. **可观测优先**：profile 触发的拒绝必须能在 validator/strict gate 中定位。

---

## 4. 回归建议（最小集）

- `test_ShaderGenPhysicalDeviceProfileJsonPath`
- `test_ShaderGenContractValidatorProfile`
- `test_RendererShaderGenAdapterProfileCategory`
- `test_MaterialPresetExhaustiveCompile`
- 示例：`03_BasicLitSunDirectionECS`

---

## 5. 已知边界

- 本矩阵当前覆盖“已接入字段”，不代表 `PhysicalDeviceProfileLite` 全字段都已绑定策略分支。
- 若新增 profile 字段，必须同步更新：
  1) apply 逻辑；2) validator 规则；3) 本矩阵；4) 至少一条回归用例。
