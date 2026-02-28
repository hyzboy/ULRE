# Runtime 硬编码白名单接入草案（C++）

本文用于落地 `ModernFixedRenderPipeline_VariantMatrix.md` 的运行时策略：

- 运行时：硬编码白名单（唯一判定来源）
- 编辑器：可选读取 JSON 做校验/可视化

---

## 1. 目标

- 给 Shader 编译入口提供稳定、零外部依赖的合法性判定
- 对非法组合与 Reserved 组合提供统一降级路径
- 与文档矩阵保持一一对应，便于维护

---

## 2. 推荐代码组织

建议新增（或集中）以下文件：

- `inc/hgl/graph/mtl/ShaderVariantWhitelist.h`
- `src/ShaderGen/ShaderVariantWhitelist.cpp`

核心职责：

1. 平台与档位映射（`Platform -> DefaultTier + AllowedTiers`）
2. 档位白名单（RenderPath / LightModel / SkyModel / SurfaceSet / Shadow）
3. Reserved 组合标记（目前至少含 `S6+` 和扩展轴）
4. 组合校验与降级策略

---

## 3. 建议类型定义（示意）

```cpp
enum class PlatformProfile : uint8
{
    MobileLow,
    MobileHigh,
    ConsolePerf,
    ConsoleQuality,
    PCLow,
    PCHigh,
};

enum class QualityTier : uint8 { Low, Medium, High };
enum class RenderPath : uint8 { ForwardVertexLit, ForwardPixelLit, MobileDeferredSubpass, GBufferDeferred, VBufferDeferred };
enum class LightModel : uint8 { BlinnPhong, PBR };
enum class SkyModel : uint8 { FlatSimple, AtmosphereSimple, SH, EnvCubeMap, Atmosphere, IBL };
enum class SurfaceSet : uint8 { S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S12, S13 };
enum class ShadowMode : uint8 { None, PCF, PCSS };

enum class RejectReason : uint8
{
    None,
    IllegalCombination,
    ReservedCombination,
};

struct VariantRequest
{
    PlatformProfile platform;
    QualityTier tier;
    RenderPath render_path;
    LightModel light_model;
    SkyModel sky_model;
    SurfaceSet surface_set;
    ShadowMode shadow_mode;
};

struct VariantDecision
{
    bool accepted;
    RejectReason reason;
    VariantRequest fallback;
};
```

---

## 4. constexpr 白名单表（示意）

```cpp
struct TierWhitelist
{
    QualityTier tier;
    std::span<const RenderPath> render_paths;
    std::span<const LightModel> light_models;
    std::span<const SkyModel> sky_models;
    std::span<const SurfaceSet> surface_sets;
    std::span<const ShadowMode> shadow_modes;
};

struct PlatformTierRule
{
    PlatformProfile platform;
    QualityTier default_tier;
    std::span<const QualityTier> allowed_tiers;
};
```

建议用 `constexpr std::array` 固化：

- `kPlatformTierRules`
- `kTierWhitelists`
- `kReservedSurfaceSets`（初始：`S6..S13`）

---

## 5. 校验顺序（必须固定）

1. `Platform-Tier` 合法性（请求档位是否允许）
2. `TierWhitelist` 成员校验（每个轴是否在白名单）
3. `Reserved` 校验（例如 `S6+`）
4. Composer 托管校验（`Mask/DitherMask` 只允许 Composer 注入）

返回：

- `accepted=true`：直接使用请求组合
- `accepted=false`：附带 `reason` 与 `fallback`

---

## 6. 降级策略（建议）

- 原则：优先保持 `render_path` 不变，再降其它轴
- SurfaceSet 默认回退：`S3`
- Sky 回退顺序：`Atmosphere -> AtmosphereSimple -> SH -> FlatSimple`

示例（伪代码）：

```cpp
VariantDecision ValidateOrFallback(const VariantRequest& req)
{
    if (!IsTierAllowed(req.platform, req.tier))
        return RejectWithFallback(req, RejectReason::IllegalCombination, GetDefaultTier(req.platform));

    if (IsReservedSurfaceSet(req.surface_set))
        return RejectWithFallback(req, RejectReason::ReservedCombination, SurfaceSet::S3);

    if (!IsAllowedByTier(req))
        return RejectWithFallback(req, RejectReason::IllegalCombination, ChooseNearestLegal(req));

    return {true, RejectReason::None, req};
}
```

---

## 7. 日志约定（建议）

统一日志前缀：`[VariantWhitelist]`

- 接受：
  - `[VariantWhitelist][Accept] key=...`
- 非法拒绝：
  - `[VariantWhitelist][Reject][Illegal] requested=... fallback=...`
- Reserved 拒绝：
  - `[VariantWhitelist][Reject][Reserved] requested=... fallback=...`

要求：日志必须可区分 `Illegal` 与 `Reserved`，与 `NEXT_STEPS` 验收一致。

---

## 8. 与编辑器 JSON 的关系

- Runtime：不读取 JSON
- Editor：可读取 `ModernFixedRenderPipeline_VariantMatrix.draft.json` 做预检查
- 一致性建议：提供一个 debug 工具将硬编码表导出为 JSON，再与 draft 做 diff

---

## 9. 最小落地清单

- [ ] 定义 `VariantRequest/VariantDecision`
- [ ] 写死 `kPlatformTierRules`
- [ ] 写死 `kTierWhitelists`（Low/Medium/High）
- [ ] 写死 `kReservedSurfaceSets`
- [ ] 接入 Shader 编译入口（`CompileComposedBusinessMaterial` 前）
- [ ] 接入统一日志
- [ ] 新增 3 类测试：Accept / IllegalFallback / ReservedFallback

---

## 10. 变更历史

- 2026-02-28：初版草案，配套现代固定管线矩阵文档。