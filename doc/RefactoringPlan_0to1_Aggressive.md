# 材质系统 0到1 激进重构计划 (Aggressive Refactoring Plan)

> **背景与原则**：
> 本计划基于 6th 宪法 (material-recipe-and-materialization-spec.md) 和 _5th 分支的经验教训制定。
> **核心宗旨**：彻底抛弃历史包袱，不做任何向下兼容。该删的旧代码毫不留情地清场，从0到1构建出以 MaterializationSpec 为唯一枢纽的纯净管线。

---

## 核心架构理念
1. **单向流水线 (Compiler Pipeline)**：Recipe (源码) -> ECS (编译器前端/策略) -> Spec (IR/防火墙) -> ShaderGen (编译器后端/无脑执行) -> GLSL/SPV (目标代码)。
2. **唯一枢纽 (Spec is Law)**：MaterializationSpec 是前后端之间唯一的、自包含的契约，同时也是 Shader 的缓存键。
3. **数据供给解耦 (Double Pools)**：摒弃旧版的 Per-Material Descriptor Binding。引入全局 Bindless 纹理数组 (Set 4) 和 具名参数池 (Set 2)，通过 ECS 构建的 TextureLayer 和 DataIndex SSBO (Set 3) 进行每实例间接寻址。

---

## Phase 1: 焦土清理 (Scorched Earth)
**目标**：彻底删除当前 6th 分支中阻碍新宪法落地的“伪终态”和兼容性残余结构。

**执行清单**：
- [ ] **删除预设系统**：清理 MaterialPresetDef、MaterialVariantKey、DeviceQualityProfile 体系。
- [ ] **清理旧的 MaterialLibrary**：删除 src/ShaderGen/3d/M_*.cpp (硬编码的预设实现) 及相关注册逻辑。
- [ ] **清理 ECS 旧批处理**：重写或删除 PrimitiveBatchPipeline 中强依赖旧版 Material* 和 Pipeline* 攒批的逻辑，移除针对旧体系的 MaterialInstanceAssignmentBuffer。
- [ ] **清理 Shader 兼容层**：删除 SamplerGLSLEmitter、material_instance_ssbo.glsl、以及 Shader 中针对 TextureBaseColor 等固定 Descriptor 的声明代码。
- [ ] **清理绑定代码**：在 NewDescriptorSetLayoutFactory 中去除基于旧版理念的固定纹理 binding (例如 binding 1-6 分给 Albedo/Normal 等的设定)。

---

## Phase 2: 确立核心契约 (Core Contracts)
**目标**：用纯 C++ 数据结构定义新世界的基石，严格遵循宪法设计。

**执行清单**：
- [ ] **定义枚举体系**：
  - SSBOCategory: TransformID, TransformData, TextureLayer, DataIndex, PBRSurface 等。
  - TextureSlot: BaseColor, Normal, MR... 
  - DataSlot: PBRSurface...
  - NormalDecode: Direct, ReconstructRG8...
- [ ] **定义 MaterialRecipe**：
  - 纯意图表达：包含 MaterialModel (如 PBR_LIT)、ResourceDecl 列表 (语义名、路径、通道映射、	reat_as_constant 等) 和 StructRef 列表。无 Vulkan 运行时句柄。
- [ ] **定义 MaterializationSpec**：
  - 机器决断结果：包含 ResolvedResource (携带 Bindless Handle, TextureSlot, NormalDecode 等)、ResolvedStructRef (携带 DataSlot 等)。
  - 包含 	ex_per_instance、structs_per_instance (stride)。
  - 提供 Hash() 接口作为唯一的管线缓存键。

---

## Phase 3: Bindless与双池基础设施 (Double Pool & Bindless)
**目标**：打通数据从 CPU 到 GPU 的全局无绑定供给通道。

**执行清单**：
- [ ] **Bindless 纹理池 (Set 4)**：
  - 实现 globalTex2D[]、globalTex2DArray[] 等全局数组的绑定管理。
  - 实现 TexturePathRegistry：根据文件路径去重，加载纹理并返回带有类型安全的 Handle (如 Tex2DHandle)。
- [ ] **结构体池 (Set 2)**：
  - 构建开放注册的 SSBO 池管理器 (如 PBRSurface 缓冲)，供多个 Instance 复用同一套参数 (如“湿润大理石”共享参数块)。
- [ ] **间接索引表 (Set 3)**：
  - 在 Vulkan 侧建立支持每帧动态写入的 TextureLayer (uint32 数组) 和 DataIndex (uint32 数组) SSBO。

---

## Phase 4: ECS 前端决策 (Probe & Resolve)
**目标**：ECS 彻底接管渲染决策权，将复杂的场景状态坍缩为极简的 Spec。

**执行清单**：
- [ ] **实现 Resource Probe**：轻量级解析纹理文件头，获取 Format、Channel Count，供策略使用。
- [ ] **实现 MaterializationResolver**：
  - 整合 Recipe + 场景画质/条件 + Probe 元数据。
  - 执行条件编译、优化降级 (如远距离无视法线、将纯色贴图 Reclassify 为 Constant/BakedResource)。
  - 输出确定性的 MaterializationSpec。
- [ ] **重构 ECS Batching**：
  - 渲染攒批 (Batch Key) 由旧版的 MaterialPipelineKey 彻底改为 Spec Hash + Geometry。
- [ ] **组装间接表**：
  - FinalizeBatch 阶段，遍历 Instance，按照 Spec 定义的 	ex_per_instance 步长填充 TextureLayer，按照 structs_per_instance 步长填充 DataIndex。

---

## Phase 5: 编译器后端 (ShaderGen)
**目标**：Shader 生成器变为“无脑执行器”，完全由 Spec 驱动。

**执行清单**：
- [ ] **改造 CompositorAssembler**：
  - 废弃针对 MaterialPreset 的组装，改为输入 MaterialModel + MaterializationSpec。
- [ ] **动态生成 GetPBRInput()**：
  - 使用 Inja (或手写字符串拼接) 解析 Spec 中的 ResolvedResource。
  - 对 BakedResource 生成常数赋值 (如 m.normal = vec3(0,0,1);)。
  - 对 BoundResource 生成带间接寻址的采样代码 (如 	exture(globalTex2D[texlayer.tex_layers[tex_base + 0]], uv))。
  - 根据 NormalDecode 插入法线解码算法。
- [ ] **重写 SPVCache**：
  - 缓存键变更为 Spec Hash (+ 渲染 Pass)。确保相同 Spec 精准复用同一个 Shader。

---

## Phase 6: 端到端验证 (E2E)
**目标**：第一条纵向切片落地，证明理论的可行性与优越性。

**执行清单**：
- [ ] **重构基础 Demo** (BasicLitMeshesECS 等)：
  - 使用纯 MaterialRecipe 构建场景。
- [ ] **验证元数据分离变体**：
  - 测试同一个 Recipe，分别使用 RGB16F 和 RG8 两种不同格式的法线贴图，断言 ECS 是否正确产生了 2 个不同的 Spec，并触发生成了 2 个不同的 Shader。
- [ ] **验证参数复用**：
  - 测试 100 个实体，其中 50 个使用“参数A”，50 个使用“参数B”（引用同一 PBRSurface 类型的不同实例）。断言它们是否合并在同一个 Spec/DrawCall 下，仅 DataIndex 值不同。
- [ ] **验证平滑降级**：
  - 标记某张贴图为 	reat_as_constant=true，断言生成的 GLSL 中是否自动消除了 	exture() 采样，变成了常数硬编码，且 TextureLayer stride 自动减小。
