# Sprite2D 迁移 — Step 6：逐个替换调用点

> 状态：进入"破坏性"阶段；每改一个调用点必须保证全量构建 + 该示例视觉 OK
> 风险等级：高
> 预计耗时：1–2 天（视调用点数量）
> 关键原则：**一次只改一个调用点 → 编译 → 跑对应示例 → commit**。绝不批量替换。

---

## 0. 目标

把工程内除"旧示例 + 旧 group + 旧 component + 旧 system"外的、所有引用以下符号的代码替换为 Sprite2D 版本：

- `QuadComponent` → `Sprite2DComponent`
- `BillboardRenderPipelineGroup` → `Sprite2DRenderPipelineGroup`
- `CreateBillboard2DFixed/Dynamic` → `CreateSprite2D`（带 `axis_locked` 参数）
- `BillboardSizeUVec2` schema → `Sprite2DTransform` schema
- `GeometryMode::BillboardCameraFacing/AxisLocked` → `Sprite2DCameraFacing/AxisLocked`

> **本步暂不删除**旧符号定义（`QuadComponent` 类、`BillboardRenderPipelineGroup` 等仍保留）。删除是 Step 7 的事。

---

## 1. 前置条件

- Step 1–5 全部通关。
- `03_Sprite2DPerspectiveECS` 视觉全部正确，作为新基线。

---

## 2. 调用点清单（执行前先 grep）

逐个跑：

```pwsh
# 1. Component
git grep -n "QuadComponent" -- ":!example/Basic/Sprite2DTest_use_ECS.cpp" `
                              ":!doc/Sprite2D_*.md" `
                              ":!**/QuadComponent.*" > step6_quadcomp.txt

# 2. Group
git grep -n "BillboardRenderPipelineGroup" -- ":!**/BillboardRenderPipelineGroup.*" `
                                              ":!doc/Sprite2D_*.md" > step6_group.txt

# 3. Factory
git grep -nE "CreateBillboard2D(Fixed|Dynamic)" > step6_factory.txt

# 4. Schema
git grep -n "BillboardSizeUVec2" > step6_schema.txt

# 5. Enum
git grep -nE "GeometryMode::Billboard(CameraFacing|AxisLocked)" > step6_enum.txt
```

把这 5 份清单合并成 **`doc/Sprite2D_Step6_调用点清单.md`**（手动维护），打勾推进。

> 旧 `BillboardRenderPipelineGroup.cpp/h`、`QuadComponent.cpp/h`、旧示例本身的内部引用不算调用点（这些 Step 7 整文件删）。

---

## 3. 执行节奏（关键：单点推进）

对清单上的每一项，按以下流程：

```
1. 切到独立 commit；改动范围只限当前文件 / 当前函数
2. 全量构建 cmake --build build --config Debug
3. 跑该改动覆盖的示例（可能是 02_BillboardECS、05_FacingMeshBillboardECS 等）
4. RenderDoc 抓 1 帧，与 Step 5 基线视觉对比
5. 通过 → commit message: "step6: migrate <file>::<func> Quad→Sprite2D"
6. 失败 → 立刻 git restore，分析问题，再单点重试
```

**禁止**：
- 一次 commit 改 5 个文件
- 跳过对应示例的视觉验证
- "看起来跟 Step 5 一样就行"，必须截图比对

---

## 4. 典型替换模式

### 4.1 QuadComponent → Sprite2DComponent

```diff
- auto q = entity->AddComponent<QuadComponent>();
- q->SetPixelSize({256, 256});
- q->SetTexturePath(OS_TEXT("res/image/lena.Tex2D"));
+ auto s = entity->AddComponent<Sprite2DComponent>();
+ s->SetFixedSize(true);
+ s->SetPixelSize({256, 256});
+ s->SetPivot({0.5f, 0.5f});               // ← 新字段，给默认值
+ s->SetRotation(0.0f);                    // ← 新字段
+ s->SetTint({255,255,255,255});           // ← 新字段
+ s->SetTexturePath(OS_TEXT("res/image/lena.Tex2D"));
```

### 4.2 BillboardRenderPipelineGroup → Sprite2DRenderPipelineGroup

```diff
- ecs::EnsureSystemGroupSystems(ctx, "Billboard", default_rt);
+ ecs::EnsureSystemGroupSystems(ctx, "Sprite2D", default_rt);
```

或直接调用：

```diff
- BillboardRenderPipelineGroup::Install(ctx, default_rt);
+ Sprite2DRenderPipelineGroup::Install(ctx, default_rt);
```

### 4.3 CreateBillboard2DFixed/Dynamic → CreateSprite2D

```diff
- auto mat = mtl::CreateBillboard2DFixed(profile, &cfg);
+ mtl::Sprite2DMaterialCreateConfig sp_cfg{};
+ sp_cfg.axis_locked       = true;
+ sp_cfg.fixed_size        = true;
+ sp_cfg.use_texture_array = cfg.use_texture_array;
+ sp_cfg.base_color_channel= cfg.base_color_channel;
+ sp_cfg.blend_mode        = cfg.blend_mode;
+ auto mat = mtl::CreateSprite2D(profile, &sp_cfg);
```

### 4.4 GeometryMode 枚举

```diff
- key.geometry_mode = mtl::GeometryMode::BillboardCameraFacing;
+ key.geometry_mode = mtl::GeometryMode::Sprite2DCameraFacing;
```

---

## 5. 风险点与应对

| 风险 | 应对 |
|---|---|
| 同一 entity 同时存在 `QuadComponent` 与 `Sprite2DComponent` | 替换时彻底删除旧 add，不要图省事保留旧的"以防万一" |
| 旧调用点期望"1 顶点扩 4 角"行为 | 新 unit-square 的 `front_face` 默认 CW，与旧 Billboard 顺序一致；如果出现镜像，把 `SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)` |
| `BillboardSizeUVec2` 还被某个 `MaterialResolveSystem` 路径写入 | grep 一下 `_pad`、`size_pixel`、`size_world` 等旧 field 名，整段重写 |
| 旧示例未被改但仍能跑（属意外） | 验证旧示例确实走旧 Billboard 路径，没"被串"到 Sprite2D；对比 Step 5 基线截图 |
| 调用点改完后该示例 camera UBO 全 0 | 检查改动是否误把 `BillboardRenderPipelineGroup::Install` 整体替换成只调用 `RegisterRenderSystem`，丢了 `EnsureCoreEcsSystems` |

---

## 6. 自检清单（每改完一个调用点）

- [ ] 全量构建 0 error 0 warning。
- [ ] 改动覆盖的示例视觉与基线一致；如有差异必须能用 pivot/rotation/tint 默认值解释。
- [ ] RenderDoc：mi_data SSBO stride = 32B（不再是旧 BillboardSizeUVec2 的 16B）。
- [ ] ECS profiler 中**没有**新旧两个 binding system 同时跑（`Sprite2DMaterialBindingSystem` ✓，`QuadMaterialBindingSystem` 无）。
- [ ] commit message 单一明确。

---

## 7. 完成标准

- 清单 5 份文件全部空（grep 结果 0 行）—— **除以下白名单外**：
  - 旧 `BillboardRenderPipelineGroup.{h,cpp}` 文件内部
  - 旧 `QuadComponent.{h,cpp}` 文件内部
  - 旧 `QuadResourcePrepareSystem.{h,cpp}` / `QuadMaterialBindingSystem.{h,cpp}` 文件内部
  - 旧 `M_BillboardFixedSize.cpp` / `M_BillboardDynamicSize.cpp` 文件内部
  - 旧 `MaterialVariantKey.h` 中保留的 `BillboardCameraFacing/AxisLocked` 枚举值
  - 文档目录 `doc/Sprite2D_*.md`
  - 旧示例（如还存在）
- 这些白名单条目在 Step 7 中整体删除。
- 所有保留的示例（包括非 Sprite2D 的 02_*、05_* 等）跑通且视觉与 Step 5 基线一致。

---

## 8. 回滚方案

由于 Step 6 是一系列**小颗粒度** commit，回滚方式：

```pwsh
# 回滚单个错误 commit
git revert <commit-hash>

# 或直接回到 Step 5 末尾
git reset --hard <step5-tag>
```

> 强烈建议在 Step 5 通关后打 tag：`git tag sprite2d-step5-baseline`。

---

## 9. Step 6 通关条件

- [ ] 调用点清单全部清零（除白名单）。
- [ ] 全部 commit 在分支上呈现可阅读的渐进过程。
- [ ] 所有保留示例视觉零退化。
- [ ] 全工程构建 0 error 0 warning。
- [ ] 本 .md 末尾追加 `已通过：YYYY-MM-DD by xxx`，附调用点清单文件路径。
