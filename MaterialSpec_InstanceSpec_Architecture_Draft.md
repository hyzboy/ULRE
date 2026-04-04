# 材质规格化架构草案（MaterialSpec / MaterialInstanceSpec）
日期: 2026-04-04  
状态: Draft v0.2

## 1. 目标与原则
### 1.1 目标
1. 用户层只声明材质需求与实例数据，不直接操心对象构建细节。
2. 渲染层将规格编译为真实 Material 与 MaterialInstance，并负责缓存复用。
3. Pipeline 继续由底层按请求合成，业务层只提供需求数据。

### 1.2 核心原则
1. 数据与对象分离：Spec 是输入，Material/MI 是产物。
2. 结构性参数入 Key。
3. 高频参数不入 Key，只写入实例数据。
4. 用户 API 尽量只暴露 Spec + Data，不暴露底层对象创建流程。

## 2. 概念模型
### 2.1 MaterialSpec（材质规格，不可变）
描述材质模板身份：
1. Preset/VariantKey。
2. SurfaceType/GeometryMode。
3. Descriptor 语义需求。
4. Shader 功能位与静态分支。

产物：可缓存、可复用的 Material。

### 2.2 MaterialInstanceSpec（实例规格，低频）
描述实例结构：
1. MaterialHandle。
2. VIL 或 VILConfig 指纹。
3. RenderPreset。
4. ResourceDomain（可选）。
5. 低频绑定结构（如数组纹理模式约束）。

产物：可缓存、可复用的 MaterialInstance。

### 2.3 MaterialInstanceData（实例数据，高频）
描述参数值：
1. base_color。
2. metallic、roughness、normal_scale。
3. 其他 UBO/SSBO 数据块。

产物：写入实例数据区，不重建对象。

## 3. Key 设计草案
### 3.1 MaterialSpecKey（建议包含）
1. Preset 或 VariantKey。
2. LightingModel。
3. 影响 Shader 代码路径的 feature bits。
4. 影响 descriptor 布局的结构性配置。

### 3.2 MaterialSpecKey（不建议包含）
1. 每帧变化参数。
2. 单个实例参数值。

### 3.3 MaterialInstanceSpecKey（建议包含）
1. MaterialHandle。
2. RenderPreset。
3. VIL 指纹。
4. ResourceDomain 标识。

### 3.4 MaterialInstanceSpecKey（不建议包含）
1. base_color/metallic/roughness 等高频数值。
2. 每实例纹理层号（建议作为 MIT 数据）。

## 4. API 草案（面向用户）
### 4.1 创建与获取
1. AcquireMaterial(spec) -> MaterialHandle
2. AcquireMaterialInstance(instanceSpec) -> MaterialInstanceHandle
3. UpdateInstanceData(miHandle, dataBlob)

### 4.2 绑定与扩展
1. BindTextureBySemantic(miHandle 或 materialHandle, semantic, texture, sampler)
2. BindBufferBySemantic(handle, semantic, buffer)
3. WarmupMaterial(spec)
4. WarmupPipeline(materialHandle, preset, vil, renderTargetFormat)

## 5. 渲染层职责
1. 规格归一化与稳定哈希。
2. 分层缓存：MaterialCache、MICache、PipelineCache。
3. 生命周期管理：句柄、引用计数或租约。
4. 失效重建：设备重建、shader 更新。
5. 诊断统计：命中率、创建量、失败原因。

## 6. 迁移策略
### 阶段 1：兼容层
1. 保留现有 CreateMaterial / CreateMaterialInstance 接口。
2. 内部转调新 Acquire 路径并统计命中。

### 阶段 2：双轨并行
1. 新代码优先用 AcquireMaterial / AcquireMaterialInstance。
2. 旧接口进入维护模式。

### 阶段 3：收口
1. 样例与文档切换到规格化接口。
2. 根据统计优化 Key 字段。
3. 逐步减少直接对象创建路径。

## 7. 使用场景草案
## 场景 A：SimpleCube（基础单物体）
1. 建一个 MaterialSpec（Gizmo3D + Triangles + Camera/L2W）。
2. AcquireMaterial。
3. 建一个 MaterialInstanceSpec（默认 VIL + Solid3D）。
4. AcquireMaterialInstance。
5. 写一次实例数据并绑定给 Primitive。
6. 后续只更新 Transform。

## 场景 B：大批量同材质（ECS 批处理）
1. 一次 AcquireMaterial。
2. 按结构差异创建少量 MI 规格。
3. 每帧只更新实例数据。
4. 批处理按 Material/PipelineKey 归并。

## 场景 C：RenderToTexture + Onscreen 双 Pass
1. 共享一个 MaterialSpec。
2. Offscreen 与 Onscreen 使用不同 MaterialInstanceSpec（可不同 preset/domain）。
3. 底层按 render target format 自动解析 pipeline。
4. 业务层不感知 pipeline 对象。

## 场景 D：文本/线条/GUI 专用渲染路径
1. 子系统内部维护固定 MaterialSpec。
2. 运行时只更新实例规格和实例数据。
3. 统一由渲染层解析 pipeline，不回写业务对象状态。

## 场景 E：PBRSpheresECS（复杂：材质实例 + 纹理阵列）
参考示例: example/Basic/PBRSpheresECS.cpp

### E.1 结构分层
1. 一个 Standard 材质模板（支持 BaseColor/Normal 数组纹理模式）。
2. 100 个实例（10x10），每实例独立 PBR 参数。
3. 每实例通过 MIT 数据选择纹理层（按列）。
4. ECS 侧使用 override material 指向对应实例。

### E.2 MaterialSpec（建议）
1. preset: Standard
2. primitive: Triangles
3. include_camera: With
4. include_l2w: With
5. include_sky: With
6. lighting_model: PBR
7. texture mode:
   - BaseColor -> Array
   - Normal -> Array

### E.3 MaterialInstanceSpec（建议）
1. material_handle: 上述 Standard 模板
2. render_preset: Solid3D
3. vil: 默认或指定
4. resource_domain: 可选（大场景可分域）

### E.4 MaterialInstanceData（建议）
1. base_color（统一灰）
2. metallic（按列梯度）
3. roughness（按行梯度）
4. normal_scale（统一或分布）

### E.5 MIT 数据（建议）
1. BaseColor layer = col
2. Normal layer = col

### E.6 该场景的 Key 边界
1. 入 MaterialSpecKey：
   - Standard + PBR + Array 模式等结构性字段
2. 不入 MaterialSpecKey：
   - metallic/roughness 具体值
3. 入 MaterialInstanceSpecKey：
   - material handle + preset + vil + domain
4. 不入 MaterialInstanceSpecKey：
   - 每实例层号和参数值（建议仅做实例数据）

### E.7 价值
1. 验证模板复用 + 多实例参数模式。
2. 验证数组纹理语义分离是否正确。
3. 验证缓存命中与批处理稳定性。

## 8. 指标与验收标准
1. Material 缓存命中率高于 95%（同场景长稳态）。
2. MI 创建量与实体规模线性，但帧间新增接近 0。
3. Pipeline 命中率在稳定场景持续上升。
4. 无因参数更新导致的对象重建抖动。
5. 无 descriptor 绑定错位与层索引错位。

## 9. 风险与规避
1. 风险：Key 字段过多导致组合爆炸。
   - 规避：只放结构性字段。
2. 风险：生命周期管理复杂。
   - 规避：句柄化和域级析构约束。
3. 风险：调试定位困难。
   - 规避：输出 spec hash 与命中日志。

## 10. MVP 落地建议
1. 新增 MaterialSpec / MaterialInstanceSpec 数据结构。
2. MaterialManager 新增 AcquireMaterial / AcquireMaterialInstance。
3. 旧接口内部转调新接口，保持兼容。
4. 接入基础统计（命中率、创建量、重建次数）。
5. 先迁移 2 个示例：
   - SimpleCube
   - PBRSpheresECS
