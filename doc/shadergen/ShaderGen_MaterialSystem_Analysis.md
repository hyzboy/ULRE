# ULRE Shader 生成器与材质系统设计分析

## 1. 分析范围与结论摘要

本文基于以下模块进行分析：
- `src/ShaderGen/*`
- `inc/hgl/shadergen/*`
- `inc/hgl/mtl/*`
- `ShaderLibrary/*`

结论：当前系统已经形成了较完整的“材质描述 -> GLSL 组装 -> SPV 编译 -> 运行时绑定契约”的流水线，且在语义类型化（UBO/SSBO semantic 分离）与布局宏自动注入方面走在正确方向上；但仍存在几个核心问题：
- 描述符/材质编译对象的生命周期管理偏手工，内存所有权不够清晰。
- 变体键（`MaterialVariantKey`）与旧预设工厂映射并存，路由层有重复和弱一致风险。
- Compositor 模板路由包含“预留/未实现 pass”路径，错误发现时间点偏晚（运行期组装时）。
- 部分 API 的输入约束未被硬性防御（例如 `SetMaterialInstance(data_bytes=0)`）。

建议优先级：
1. **P0（立即）**：补齐健壮性与生命周期管理。
2. **P1（短期）**：收敛变体路由，建立可验证的注册与测试矩阵。
3. **P2（中期）**：提升增量编译/缓存与并行编译能力。
4. **P3（长期）**：向“离线可再现产物 + 运行时轻装载”演进。

---

## 2. 当前设计主链路

## 2.1 数据与职责分层

可以抽象为 5 层：

1. **材质定义层（静态定义）**
- 代表类型：`FixedMaterialDef`、`FixedUBODescriptors`、`FixedSSBODescriptors`、`FixedTextureSamplerDescriptors`
- 作用：声明顶点输入、描述符语义、MI 结构信息。

2. **材质创建层（编译输入对象）**
- 代表类型：`MaterialCreateInfo`、`MaterialCreateConfig`（2D/3D 派生）
- 作用：收集 shader stage、descriptor 数据库、最终 GLSL、编译输出 SPV。

3. **布局契约层（布局数字解耦）**
- 代表类型：`MaterialDescriptorInfo`、`ShaderLayoutContract`
- 作用：集中分配 set/binding/location；再生成 `#define XXX_SET/XXX_BINDING/XXX_LOCATION`。

4. **模板组装层（Compositor）**
- 代表类型：`CompositorAssembler`、`ShaderPermutationKey`
- 作用：按 `SurfaceType/BlendMode/PassType` 选模板，注入宏，替换 surface include。

5. **变体路由层（高层入口）**
- 代表类型：`MaterialVariantKey`、`VariantRegistry`、`MaterialLibrary` 工厂映射
- 作用：从 preset 或 key 路由到具体 `M_*.cpp` 创建流程。

## 2.2 关键流水线

以 3D Standard 为例（`M_Standard.cpp`）：

1. 构造 `FixedMaterialDef`（含 UBO/SSBO/纹理槽与 MI）。
2. 基于 `MaterialVariantKey` 查询 `VariantRegistry` 得到模板路径。
3. `CompositorAssembler::Assemble` 读取模板 + 注入宏 + 替换 surface include，生成 VS/FS 完整 GLSL。
4. `CompileCompositorMaterial` 将定义写入 `MaterialCreateInfo`：
   - `AddUBO/AddSSBO/AddTextureSampler`
   - `SetMaterialInstance/SetLocalToWorld`
   - 绑定契约 `BindingContract`
5. `Resort -> BuildShaderLayoutContract -> EmitShaderLayoutDefines`，再把 layout defines 重新注入 GLSL。
6. `CreateShaderDirect` 调用 `GLSLCompiler` 直接编译为 SPV。

这条链路的优点是“信息闭环”完整：语义 -> set/binding -> GLSL 宏 -> 编译，避免手写硬编码 binding。

---

## 3. 设计优点（值得保留与强化）

## 3.1 语义类型化方向正确

`UBODescriptorSemantic` 与 `SSBODescriptorSemantic` 拆分后，语义意图更清晰，减少了旧式通用 semantic 的歧义；`DescriptorSemanticMeta` 将 set type / macro 名称 / struct 名集中，便于统一维护。

## 3.2 布局宏自动注入降低了“数字散落”

`ShaderLayoutBuilder + ShaderLayoutDefineEmitter` 使 GLSL 不必依赖硬编码 `layout(set=?,binding=?)` 常量，降低改 binding 时的全链路改动成本。

## 3.3 MaterialVariantKey 已具备扩展能力

`MaterialVariantKey` 支持按 sampler slot 的 2-bit source mode（None/Simple/Array/Atlas），以及顶点属性位和额外 feature 位，为后续材质规模增长提供了足够容量。

## 3.4 兼容层设计可控

`MaterialLibrary` 仍支持 `MaterialPreset`，同时可向 `VariantKey` 过渡，这对于渐进迁移非常实用。

---

## 4. 主要问题与风险

## 4.1 生命周期/所有权边界不清晰（高风险）

现象：
- `MaterialDescriptorInfo` 内部保存了大量裸指针（`UBODescriptor*` / `SSBODescriptor*` / `Texture*`）。
- `MaterialDescriptorInfo` 析构为默认实现，未见显式释放这些对象。
- descriptor 对象在多个 map 中被引用，所有权模型未文档化。

风险：
- 长时间运行或频繁构建材质时，存在内存泄漏风险。
- 后续重构时容易出现重复释放或悬挂指针。

建议：
- 采用单一所有权容器（如 `std::vector<std::unique_ptr<ShaderDescriptor>>`）+ 索引/裸观察指针。
- 在 `MaterialDescriptorInfo` 明确“拥有者”语义并集中析构。

## 4.2 API 输入防御不足（高风险）

现象：
- `MaterialCreateInfo::SetMaterialInstance` 中 `material_instance_max_count = ssbo_range / data_bytes`，但未硬性拒绝 `data_bytes==0`。

风险：
- 外部错误调用可导致除零未定义行为。

建议：
- 在 `SetMaterialInstance` 开头增加 `if(data_bytes==0) return false;`。
- 对 `SetDevice` 未调用时给出可观测告警（当前仅默认为 0 range）。

## 4.3 变体路由存在“双轨映射”重复（中高风险）

现象：
- 一部分逻辑在 `MaterialLibrary.cpp` 的工厂表中。
- 另一部分逻辑在 `VariantRegistry` 的内置注册中。
- 对同一材质，路由 key 可能需“canonicalize”后重试，说明键规范仍未完全统一。

风险：
- 新增材质时容易出现“注册了 desc 但没工厂”或“工厂可创建但 desc 未注册”的不一致。

建议：
- 统一为“单一事实来源（Single Source of Truth）”：
  - 要么以 `VariantRegistry` 为中心，工厂从 desc 驱动。
  - 要么以工厂注册为中心，desc 由工厂元数据自动派生。

## 4.4 Compositor 路由含未落地模板路径（中风险）

现象：
- `CompositorAssembler` 中 `main_shadow.*`、`main_earlyz.*` 仍标注后续实现。
- `GetPassTypesForBlendMode` 对 Opaque/Masked 默认返回 shadow/earlyz pass。

风险：
- 若调用方按 pass 列表批量编译，将在运行期组装失败。

建议：
- 在启动时做模板可用性自检（扫描 registry + pass 路由）。
- `GetPassTypesForBlendMode` 增加 capability 参数，仅返回当前可编译 pass。

## 4.5 Shader 编译服务为全局状态（中风险）

现象：
- `GLSLCompiler.cpp` 使用大量全局静态状态（include path、profile、编译参数）。

风险：
- 多线程并发编译时存在竞态风险。
- 多设备 profile 并存场景下不易隔离。

建议：
- 中期引入 `CompilerContext` 实例化上下文，避免共享可变全局状态。

---

## 5. 优化建议（按优先级）

## 5.1 P0：立即可做（1~2 周）

1. 修复 `SetMaterialInstance` 的 `data_bytes==0` 防御。
2. 给 `MaterialDescriptorInfo` 增加明确析构逻辑，先止血内存泄漏。
3. 增加“启动期材质编译自检”：
- 遍历 `VariantRegistry` 全部条目。
- 校验模板文件存在。
- 可选执行 dry-run 编译（只到 GLSL 组装或到 SPV）。

## 5.2 P1：短期演进（2~4 周）

1. 收敛路由：统一 `VariantRegistry` 与 `MaterialLibrary` 双轨注册。
2. 建立快照测试：
- 对关键材质输出 `ShaderLayoutContract` 快照。
- 对生成 GLSL 输出宏块快照。
- 防止重构引发 binding 回归。
3. 引入编译错误分级（路径不存在/semantic 不合法/编译器报错）提升定位效率。

## 5.3 P2：中期优化（1~2 个月）

1. 增量缓存：以 `VariantKey Hash + ShaderLibrary 文件指纹 + Profile` 作为缓存键。
2. 并行编译：不同 pass 或不同材质并行编译，降低启动等待。
3. 将 `DescriptorSemanticMeta` 与 `SamplerSlot` 的宏名导出为可视化表，方便工具链校验。

## 5.4 P3：长期方向

1. 建立离线烘焙管线：预编译 SPV + 反射信息 + 布局契约打包。
2. 运行时仅做“轻量选择 + 热更新”，降低现场编译依赖。
3. 与材质编辑器联动，支持可追踪的“从参数到 shader 产物”溯源。

---

## 6. 我对当前设计的看法

总体上，这套系统已经从“手工写死 shader 与 binding”升级为“语义驱动 + 自动布局 + 模板化组装”的现代形态，这是非常关键的一步，且方向正确。

最需要尽快补的是工程化“护栏”：
- 生命周期与所有权必须收口。
- 变体注册与路由必须单源化。
- 编译期/启动期验证必须前置。

把这三点做好后，你们的 ShaderGen/材质系统会从“可用”进入“可扩展且可维护”的阶段，后续再推进离线烘焙和并行编译，性价比会非常高。

---

## 7. 建议的落地顺序（可执行）

1. 先做 P0（内存/防御/自检），保证稳定性。
2. 再做 P1（单源注册 + 快照测试），保证可维护性。
3. 然后做 P2（缓存/并行），保证性能。
4. 最后做 P3（离线化），保证规模化能力。

> 这样安排的核心是先处理“正确性与回归风险”，再处理“效率与长期成本”。
