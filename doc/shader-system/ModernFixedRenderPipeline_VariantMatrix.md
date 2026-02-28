# 现代固定管线 Shader 生成器：平台 × 档位 × 组合矩阵

本文是 [ModernFixedRenderPipeline.md](ModernFixedRenderPipeline.md) 的配套白名单文档，用于约束可编译 Shader 变体集合，避免排列爆炸。

机读草案（供编译器配置接入）：[ModernFixedRenderPipeline_VariantMatrix.draft.json](ModernFixedRenderPipeline_VariantMatrix.draft.json)

## 1. 目标

- 明确各平台可用渲染路径与质量档位
- 明确各档位可用光照模型、环境光模型与材质输入集
- 作为编译白名单与运行时校验依据

## 2. 术语

- Render Path：ForwardVertexLit / ForwardPixelLit / MobileDeferredSubpass / GBufferDeferred / VBufferDeferred
- Light Model：BlinnPhong / PBR
- Sky Model：FlatSimple / AtmosphereSimple / SH / EnvCubeMap / Atmosphere / IBL
- Surface Set：
  - S1 = BaseColor
  - S2 = BaseColor+Normal
  - S3 = BaseColor+Normal+Roughness
  - S4 = BaseColor+Normal+Roughness+Metallic
  - S5 = BaseColor+Normal+Roughness+Metallic+AO

> 注：S1~S5 为当前已实现最小集；后续扩展以 **Google Filament 材质语义** 为参考，不按 Unreal 规则扩展。

## 2.1 Surface Set 扩展预留（Filament 参考，先占位）

为兼容后续更复杂材质体系（参考 Google Filament 常见输入语义），预留如下 Surface 通道集合。  
这些组合当前不纳入编译白名单，仅作为文档与配置结构占位。

### A 组：Filament Lit 常用扩展

- S6 = BaseColor + Normal + Roughness + Metallic + AO + Emissive
- S7 = BaseColor + Normal + Roughness + Metallic + AO + Emissive + Reflectance
- S8 = BaseColor + Normal + Roughness + Metallic + AO + Emissive + ClearCoat + ClearCoatRoughness + ClearCoatNormal

### B 组：Filament 高级外观扩展

- S9  = BaseColor + Normal + Roughness + Metallic + AO + SheenColor + SheenRoughness
- S10 = BaseColor + Normal + Roughness + Metallic + AO + Anisotropy + AnisotropyDirection

### C 组：Filament 透射/折射扩展

- S11 = BaseColor + Normal + Roughness + Metallic + AO + Transmission + Thickness
- S12 = BaseColor + Normal + Roughness + Metallic + AO + IOR + Absorption
- S13 = BaseColor + Normal + Roughness + Metallic + AO + BentNormal + SpecularAO

> 说明：S6~S13 当前状态均为 `Reserved`，不参与运行时合法组合校验与预编译统计。

### D 组：与 Unreal 分歧说明（规范声明）

- 不引入 Unreal 专有语义作为主规范（如 SubsurfaceProfile 工作流、Unreal ShadingModel 枚举绑定）。
- 若必须兼容外部资产，统一在导入阶段做参数映射，不改变 Surface Set 主规范。

## 2.2 其它扩展轴预留（先占位，暂不实现）

除 Surface Set 外，额外预留以下维度，后续按需求逐步开放：

- Material Domain：Surface / Decal / PostProcess / UI
- Blend Mode：Opaque / Masked / Translucent / Additive / Modulate
- Shading Model：DefaultLit / ClearCoat / Subsurface / Cloth / Hair / Eye
- Refraction Mode：None / Simple / Physical
- TwoSided：Off / On
- PixelDepthOffset：Off / On
- Dither LOD Fade：Off / On

> 说明：上述轴当前均为 `Reserved`，仅保留配置字段与文档位，不生成任何新变体。

> 额外约束（重要）：`Masked` 与 `Dither Mask` 属于 **Composer 托管能力**，不允许业务侧通过自定义 GLSL 实现。

## 3. 平台与档位映射

| 平台 | 默认档位 | 允许档位 |
|---|---|---|
| Mobile-Low | Low | Low |
| Mobile-High | Medium | Low, Medium |
| Console-Perf | Medium | Medium, High |
| Console-Quality | High | Medium, High |
| PC-Low | Low | Low, Medium |
| PC-High | High | Medium, High |

## 4. 档位白名单矩阵

> 本章仅包含“当前已启用”白名单。`2.1` 与 `2.2` 中的预留项默认不在此矩阵内。

### 4.1 Low

| 维度 | 白名单 |
|---|---|
| Render Path | ForwardVertexLit, ForwardPixelLit |
| Light Model | BlinnPhong |
| Sky Model | FlatSimple, AtmosphereSimple, SH |
| Surface Set | S1, S2, S3 |
| Shadow | None, PCF(可选) |
| 备注 | 禁用高成本屏幕空间效果 |

### 4.2 Medium

| 维度 | 白名单 |
|---|---|
| Render Path | ForwardPixelLit, MobileDeferredSubpass, GBufferDeferred(轻量) |
| Light Model | BlinnPhong, PBR(简化) |
| Sky Model | AtmosphereSimple, SH, EnvCubeMap |
| Surface Set | S2, S3, S4 |
| Shadow | None, PCF |
| 备注 | 以稳定帧率优先 |

### 4.3 High

| 维度 | 白名单 |
|---|---|
| Render Path | GBufferDeferred, VBufferDeferred |
| Light Model | PBR |
| Sky Model | Atmosphere, IBL, EnvCubeMap, SH |
| Surface Set | S3, S4, S5 |
| Shadow | None, PCF, PCSS |
| 备注 | Atmosphere/IBL 不可用时可回退 AtmosphereSimple |

## 5. 超简大气模型（AtmosphereSimple）

### 5.1 定义

`AtmosphereSimple` 是面向低成本设备的天光近似模型，设计目标是“仅一行核心公式即可给出可用天光趋势”。

参考公式（示意）：

`sky_ambient = exp2(elevation) * sky_color`

其中：

- `elevation`：太阳仰角或视线仰角映射值（建议先 clamp 到可控范围）
- `sky_color`：天空主色（可按时段插值）

### 5.2 适用场景

- 移动低端设备
- 大场景远景天光过渡
- 完整 Atmosphere/IBL 编译失败时的回退路径

### 5.3 约束

- 必须保证昼夜切换连续，不出现突变
- 必须与 SH/IBL 的色域映射保持一致（避免切换色偏）

## 6. 编译白名单规则（实现建议）

1. 启动时按 `platform + tier` 读取白名单集合
2. 生成 `PermutationKey` 前先做组合合法性校验
3. 非法组合：
   - 编辑器模式：报错并显示可替代建议
   - 运行时：降级到同材质最近合法组合（优先同 Render Path）
4. 预留项组合（Reserved）：
  - 编辑器模式：给出“功能未开放”提示，不进入编译队列
  - 运行时：强制降级到最近的已实现 Surface Set（默认 S3 或平台指定默认）

## 6.1 Mask / Dither Mask 实现归属

### 原则

- `OpacityMask`（硬裁剪）与 `Dither Mask`（抖动裁剪）由 Composition 层统一注入。
- Business Logic 层只提供通用输入（如 `opacity_mask` 值），不直接写 `discard` 或抖动噪声逻辑。
- 禁止材质模板外的自定义 mask 代码路径，避免渲染队列与深度行为失控。

### 组合轴建议（可先占位）

- AlphaCutMode：Off / HardMask / DitherMask
- AlphaCutSource：Constant / TextureA / CustomChannel(受白名单限制)

### 运行时行为（建议）

- `HardMask`：由 composer 注入固定阈值裁剪路径。
- `DitherMask`：由 composer 注入固定抖动模式（蓝噪或 Bayer），并绑定帧稳定策略。
- 不支持平台上自动回退 `HardMask` 或 `Off`（按平台配置）。

### 当前状态

- 文档层面：已定义为 Composer 托管能力。
- 实现层面：若尚未接入，则状态记为 `Experimental/Reserved`，但归属不变（仍由 composer 实现）。

## 7. 缓存键建议

缓存键至少包含：

- MaterialType
- RenderPath
- LightModel
- SkyModel
- SurfaceSet
- Platform
- Tier
- ShaderTemplateVersion

示例：

`BasicLit|GBufferDeferred|PBR|AtmosphereSimple|S4|PC-High|High|v3`

## 8. 最小验收清单

- [ ] 各平台默认档位可完成冷启动预编译
- [ ] `AtmosphereSimple` 在 Low/Medium 档位渲染稳定
- [ ] 非法组合能被拦截并正确降级
- [ ] 切换 SkyModel（AtmosphereSimple/SH/IBL）时无黑材质、无崩溃
- [ ] 变体数量符合平台预算
- [ ] Reserved 组合（Filament 扩展 S6+ 或扩展轴）可被稳定识别、拒绝并正确降级

## 9. 状态标记建议（文档与配置共用）

建议所有组合与轴都带状态字段，避免“文档写了但实现误判可用”：

- Implemented：已实现并可编译
- Experimental：实验中，仅特定分支可开
- Reserved：仅占位，不可编译
- Deprecated：保留兼容，计划移除

示例：

- S1~S5：Implemented
- S6~S13：Reserved
- Material Domain/Blend/Shading 扩展轴：Reserved
- Mask/DitherMask：ComposerOwned（状态可为 Experimental 或 Reserved）
