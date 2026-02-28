现代固定管线 Shader 生成器（实现草案）

## 1. 背景与目标

当前引擎以“极致 DrawCall 合并”为核心优化方向，通常一个 DrawCall 覆盖全场景一批不透明物件。  
在这种约束下，开放任意自定义 Shader 会导致：

- 变体失控（编译数量爆炸）
- 渲染路径不可预测（前向/延迟分裂）
- 材质兼容性与调试成本急剧上升

因此采用“现代固定管线 + 可配置生成”的模式：

- 业务开发只配置材质参数与特性开关
- Shader 逻辑模板由引擎团队维护
- 由生成器统一产出各渲染路径 Shader 变体

## 2. 核心原则

### 2.1 固定接口，非固定实现

- 对上层暴露固定的材质输入语义（BaseColor/Normal/Roughness/Metallic/AO 等）
- 对底层可切换不同渲染路径（Forward/Deferred/Subpass/VBuffer）
- 同一材质实例数据结构，映射到不同路径时保持字段语义一致

### 2.2 特性驱动生成，不允许自由拼接

- 允许配置“特性轴”
- 禁止直接注入任意 GLSL 片段
- 所有分支都必须在模板内白名单化

### 2.3 变体可控

- 变体由“有限维度排列键”决定（Permutation Key）
- 通过“质量档位 + 平台裁剪”控制总组合数

## 3. 生成维度（Permutation Axis）

建议最小可用轴如下：

1. 渲染路径（Render Path）
    - ForwardVertexLit
    - ForwardPixelLit
    - MobileDeferredSubpass
    - GBufferDeferred
    - VBufferDeferred

2. 材质输入集（Surface Feature Set）
    - BaseColor
    - BaseColor+Normal
    - BaseColor+Normal+Roughness
    - BaseColor+Normal+Roughness+Metallic
    - BaseColor+Normal+Roughness+Metallic+AO

3. 光照模型（Light Model）
    - BlinnPhong
    - PBR

4. 环境光模型（Sky/Ambient Model）
  - FlatSimple（常量或高度渐变）
  - AtmosphereSimple（超简大气：`exp2(elevation) * sky_color` 一行近似）
    - SH
    - EnvCubeMap
    - Atmosphere
    - IBL

5. 可选附加轴（按平台启用）
    - 阴影模式（None/PCF/PCSS）
    - 法线压缩编码（None/Oct/Spheremap）
    - 输出模式（单 RT / 多 RT）

## 4. 质量档位（Quality Tier）

建议预定义三档，禁止业务侧自定义组合：

- Low（移动低端）
  - ForwardVertexLit / ForwardPixelLit
  - BlinnPhong
  - FlatSimple / AtmosphereSimple / SH
  - 禁用高成本屏幕空间效果

- Medium（移动高端 / 主机性能模式）
  - MobileDeferredSubpass 或轻量 GBuffer
  - BlinnPhong 或 PBR-Lite
  - AtmosphereSimple / SH / EnvCubeMap

- High（PC/主机画质模式）
  - GBufferDeferred / VBufferDeferred
  - PBR
  - Atmosphere / IBL（可回退 AtmosphereSimple）

## 5. 材质数据与逻辑分层

建议沿用“三层分离”：

- MaterialInstance 数据层：只存参数，不含路径逻辑
- Business Logic 层：描述 VS/FS 业务语义（固定白名单函数）
- Composition 层：根据 Permutation Key 组装辅助函数、宏与模板

这样可保证：

- 同一份业务语义可复用到多渲染路径
- 测试可分别覆盖“语义正确性”和“编译可用性”

## 6. 编译与缓存策略

### 6.1 编译时机

- 启动阶段：按当前平台 + 质量档 + 场景配置批量预编
- 运行时：允许懒编译，但必须有 fallback（默认材质或低档变体）

### 6.2 缓存键（必须）

缓存键至少包含：

- 材质类型
- Render Path
- Surface Feature Set
- Light Model
- Sky/Ambient Model
- 平台标识与关键宏版本

若场景切换了天空模型（如选人界面 IBL、大场景 SH），应通过缓存键自然区分并触发对应变体编译。

## 7. 变体爆炸控制建议

1. 平台白名单裁剪：不支持的轴组合在配置阶段直接拒绝
2. 档位绑定：Low/Medium/High 固定可用组合
3. 热路径优先：对高频材质先预热，低频材质懒编译
4. 统计驱动：记录变体命中率，定期清理低命中组合

## 8. 验收标准（建议）

### 8.1 功能正确性

- 同一材质在不同渲染路径下视觉语义一致（允许可解释差异）
- 各环境光模型切换时无崩溃、无黑材质
- `AtmosphereSimple` 在低端设备上能稳定替代完整大气模型，且昼夜主色调变化连续

### 8.2 工程稳定性

- Shader 生成失败有明确诊断信息
- 编译失败可回退到可运行路径
- Gate 测试覆盖关键材质模板 conformance

### 8.3 性能约束

- 启动预编译总时长可控
- 运行时首次懒编译不会造成明显卡顿峰值
- 变体总数在平台预算内

## 9. 结论

这套方案的本质是：

- 用“固定管线”保证稳定性与可维护性
- 用“特性轴 + 生成器”保留必要灵活性

建议下一步在文档旁补一份“平台-档位-组合矩阵表”，作为实际编译白名单来源，避免后续功能新增时重新失控。

已补充配套文档：见 [ModernFixedRenderPipeline_VariantMatrix.md](ModernFixedRenderPipeline_VariantMatrix.md)。


