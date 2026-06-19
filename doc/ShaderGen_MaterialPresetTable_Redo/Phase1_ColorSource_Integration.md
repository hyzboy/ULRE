# Phase 1 技术文档：ColorSource 新通路接入（双路并存）

## 1. 阶段目标

在不破坏现有稳定行为的前提下，将 ColorSource 代码生成体系接入 `MaterialCreateInfo` 主流程，形成“新旧双路并存、可切换、可验证”的中间状态。

关键目标：

1. 新路径可用：`CodegenRegistry -> IColorSourceCodegen` 可真实产出代码。
2. 旧路径可退：`SamplerGLSLEmitter` 仍可作为兜底路径。
3. 功能可控：通过开关选择路径，方便分阶段验证。

---

## 2. 范围与边界

### 2.1 纳入

1. 新增/接入接口：`IColorSourceCodegen`。
2. 新增/接入注册中心：`CodegenRegistry`。
3. 新增内置实现：
   - `BuiltinSampler2DCodegen`
   - `BuiltinSampler2DArrayCodegen`
   - `BuiltinBindlessSamplerArrayCodegen`（可先存根）
4. `MaterialCreateInfo.cpp` 中增加新路径分派。
5. `BindingAllocator` 及单元测试接入。

### 2.2 不纳入

1. 立即删除 `SamplerGLSLEmitter`。
2. 与 Matcher/PresetTable 绑定。
3. 跨模块大范围 API 重签名。

---

## 3. 设计决策

1. “并存优先于替换”：先证明新路径稳定，再删旧路径。
2. “行为一致优先”：同一输入 recipe 下，新旧路径输出语义必须一致。
3. “差异可观察”：新增日志标记本次构建使用了哪条 codegen 路径。

---

## 4. 关键接口契约

### 4.1 IColorSourceCodegen

1. 输入：ColorSource 描述、绑定分配结果、目标 shader stage 上下文。
2. 输出：GLSL 片段（声明 + 访问函数 + 辅助宏/函数）。
3. 错误：返回可诊断错误码，不允许静默吞错。

### 4.2 CodegenRegistry

1. 以 ColorSource 类型查询对应 codegen 实现。
2. 未命中时明确返回“未注册”错误。
3. 注册顺序不应影响语义（避免隐式覆盖）。

### 4.3 MaterialCreateInfo 接入

1. 新路径失败时可回退旧路径（阶段内策略）。
2. 回退必须记录日志，包含失败原因。
3. 禁止“部分片段来自新路径、部分来自旧路径”的混合无序状态。

---

## 5. 建议改动文件

1. 头文件：
   - `inc/hgl/shadergen/ColorSource.h`
   - `inc/hgl/shadergen/IColorSourceCodegen.h`
   - `inc/hgl/shadergen/MaterialCreateInfo.h`
2. 源文件：
   - `src/ShaderGen/ColorSource/CodegenRegistry.*`
   - `src/ShaderGen/ColorSource/BuiltinSampler2DCodegen.*`
   - `src/ShaderGen/ColorSource/BuiltinSampler2DArrayCodegen.*`
   - `src/ShaderGen/ColorSource/BuiltinBindlessSamplerArrayCodegen.*`
   - `src/ShaderGen/ColorSource/BindingAllocator.*`
   - `src/ShaderGen/MaterialCreateInfo.cpp`
3. 测试：
   - `src/ShaderGen/tests/BindingAllocatorTests.cpp`
   - 新增/更新 ColorSource codegen 覆盖用例

---

## 6. 执行步骤

1. 定义并固定 `IColorSourceCodegen` 接口最小集合。
2. 建立 `CodegenRegistry` 并注册内置 codegen。
3. 在 `MaterialCreateInfo.cpp` 加入新路径分派与日志标识。
4. 增加 feature flag（或配置项）切换新旧路径。
5. 对核心材质类型跑新旧路径输出比对测试。
6. 完成回归并记录差异。

---

## 7. 验收标准

1. 编译通过，新增测试通过。
2. 在新路径打开时，核心示例渲染正确。
3. 在新路径关闭时，行为与基线一致。
4. 日志可明确判断本次使用路径与回退原因。

---

## 8. 风险与防错

1. 风险：新路径绑定号分配与旧路径不一致。
   - 防错：引入 BindingAllocator 冲突测试与快照测试。
2. 风险：Bindless 能力在不支持设备上误启用。
   - 防错：能力检测未通过时降级到非 bindless codegen。
3. 风险：阶段内过早删除旧路径导致大面积回归。
   - 防错：本阶段严格禁止删除 `SamplerGLSLEmitter`。

---

## 9. 回滚方案

1. 快速回滚方式：关闭新路径 feature flag。
2. 结构回滚方式：回滚 Phase 1 相关提交到 Phase 0 tag。
3. 若仅个别 codegen 不稳定，可禁用该实现并保留框架。

---

## 10. 阶段完成定义（DoD）

1. ColorSource 新路径接入完成，可切换可验证。
2. 旧路径保留且可作为回退。
3. 测试与日志能够证明新路径行为可控。
4. 允许进入 Phase 2。