# Sprite2D 重做前的前置工作建议

如果回退到 Sprite2D 之前重新来过，下面这些「基础设施统一」工作应该**先于 Sprite2D 业务代码**完成。每一项都能消除我们这次踩过的某个 BUG 类。

---

## 一、顶点输入层（最高优先级）

我们这次最痛的就是 vec2/vec3 Position 混用。

1. **`VertexAttrib::Position` 引入维度变体**
   - 当前 `VertexAttrib` 是单一枚举，hash key 里没有「Position 是 vec2 还是 vec3」的位。
   - 这次只能用 `ExtraFeature::Vec2Position` 这种打补丁方式塞进 `extra_bits`，导致：
     - 注册表条目要排字段顺序（C7560）
     - `MakeXxxKeyBase` 必须显式 `SetVec2Position(true)`
     - 任何忘记设置的地方都 hash 不上
   - **重做方案**：在 `VertexAttrib` 或 `MaterialVariantKey` 里把 Position 维度作为「正式字段」管理，而不是 extra bit。

2. **`VertexInputDescriptor` / `VF_V2F` vs `VF_V3F` 自动派生**
   - 让 GeometryHelper 根据 GeometryMode 自动选择 Position 格式，避免业务层传错。

3. **TexCoord 默认开启的 GeometryMode 标记**
   - 这次 `MakeSprite2DKeyBase` 一开始多调了 `SetVertexAttribEnabled(TexCoord)`，hash 立刻和 registry 错开。
   - 应在 `GeometryMode` 表里声明「这种几何天然带哪些 vertex attrib」，由系统自动 set，业务层不再手动加位。

---

## 二、ShaderDataSchema 注册机制

这次 `schema=7` 跑出来 `bytes=0`，因为 `schema_sprite2d_transform.glsl` 文件根本不存在，但代码里 `ShaderDataSchema::Sprite2DTransform=7` 已经登记。

1. **Schema 启动期校验**
   - 启动时遍历 `ShaderDataSchema` 全部条目，逐个 `LoadFile + ParseStruct`，缺文件或 size=0 直接 `Fatal`，不要等到第一帧渲染才发现。
2. **C++ 结构体 ↔ GLSL struct 一致性 static_assert**
   - 用 `static_assert(sizeof(Sprite2DTransform)==32, ...)` 配合 schema 解析结果做运行期对照，错位/漏 pad 立刻报错。
3. **Schema 文件命名/路径常量化**
   - 把 `"schema_sprite2d_transform.glsl"` 这种字符串集中管理，避免代码里登记了名字但忘了建文件。

---

## 三、`MaterialPreset ↔ Variant` 连接表

这次新增 preset 26/27 时改了 4 处：`MaterialPreset` 枚举、`kPresetResolveTable`、`kBuiltinVariants`、`MaterialLibrary` 的 `MakeXxxKey` 工厂；漏一处就 `lookup failed`。

1. **统一定义在一张表**
   - 用 `X-Macro` 或单一 `kPresetTable[]` 同时生成枚举值、resolve 表、make_key 函数指针，写一处。
2. **启动期反向校验**
   - 已经有 `PresetResolveTable validation passed`，再加一项「每个 preset 都能找到至少一个 builtin variant」的反向校验。

---

## 四、`BuiltinVariantEntry` 的可维护性

C7560 是 designated initializer 顺序错误，10 处都得改。

1. **结构体字段顺序按使用频率重排**
   - 频繁出现的字段（name/preset/blend/pass/extra_bits/tex/路径）放前面，少用的放后面。
2. **改用 builder 函数而非 designated initializer**
   - `BuiltinVariantEntry::Make().Preset(X).Blend(Y).Tex(Z)...` —— 顺序无关，少一个字段编译期报错。

---

## 五、Geometry / Primitive 共享策略

我们看到 26 个 sprite 实体只处理了 1 个就崩，怀疑是同一 `DescriptorSet` / `Primitive` 被多实体重复绑定。Billboard 时代是「每实体一份」，Sprite2D 想做共享，但共享语义没先定。

1. **明确「材质实例 vs 几何 vs primitive」共享层级**
   - 重做前先定：相同纹理的 sprite 是否共享 `MaterialInstance`？是否共享 `Primitive`？`DescriptorSet` 由谁拥有？
2. **`DescriptorSet::BindResourceSampler` 幂等化**
   - 当前同 binding 二次 bind 直接 `false`。改成「相同 tex+sampler 视为成功」或者提供 `Rebind` API。
3. **Sprite2D 用 instancing 而非 per-entity primitive**
   - 既然有 `Sprite2DTransform` MI 数组，就该走 instanced draw，不要每个 entity 一个 primitive。这从设计上消除大半同步/共享 BUG。

---

## 六、ECS Render System 的执行契约

这次 crash 在 `SetPrimitive/SetTextureObjects/SetAppliedTexturePath` 之一，根本原因可能是 system 执行顺序或 component 生命周期。

1. **System 依赖声明显式化**
   - `Sprite2DMaterialBindingSystem` 依赖 `Sprite2DResourcePrepareSystem`、`RenderDescriptorBindingSystem`，应在 system 注册时声明，由调度器排序，避免人肉保证顺序。
2. **Component setter 的线程/帧契约**
   - `SetPrimitive` 等是否能在渲染遍历中调用？是否要 deferred？先把契约写清。

---

## 推荐执行顺序

| 顺序 | 工作 | 解决的 BUG 类 |
|---|---|---|
| 1 | VertexAttrib Position 维度正式化（替代 Vec2Position extra bit） | C7560 / hash mismatch |
| 2 | GeometryMode → 自动 vertex attrib 派生 | TexCoord 多余位 |
| 3 | Preset/Variant/MakeKey 单表生成 + 反向校验 | preset lookup failed |
| 4 | ShaderDataSchema 启动期强校验 + static_assert | schema bytes=0 |
| 5 | BuiltinVariantEntry builder 化 | designated init 排序错 |
| 6 | 明确 MI/Primitive/DescriptorSet 共享语义 + instancing | 多实体崩溃、binding 冲突 |
| 7 | 才开始写 Sprite2D 业务代码 | —— |

完成 1–6 后，Sprite2D 实现本身基本只剩「写 shader + 写 component + 写 system」的纯业务，不会再触碰这次踩到的任何基础设施 BUG。