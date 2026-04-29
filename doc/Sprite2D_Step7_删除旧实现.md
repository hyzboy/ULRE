# Sprite2D 迁移 — Step 7：删除旧实现

> 状态：破坏性、不可逆；删除完成后 Billboard / Quad 符号永久从工程消失
> 风险等级：**最高**
> 预计耗时：半天 ~ 1 天
> 关键原则：删除前**先做序列化兼容**；删除按"叶子→根"顺序，每删一组立刻全量构建。

---

## 0. 目标

删除迁移方案 §4.1 列出的全部文件 / 枚举 / schema / 工厂函数：

1. 旧 ShaderGen：`M_BillboardFixedSize.cpp`、`M_BillboardDynamicSize.cpp`、对应 GLSL 模板
2. 旧 Component：`QuadComponent.{h,cpp}`
3. 旧 Systems：`QuadResourcePrepareSystem.{h,cpp}`、`QuadMaterialBindingSystem.{h,cpp}`
4. 旧 Group：`BillboardRenderPipelineGroup.{h,cpp}`
5. 旧 Material 工厂：`CreateBillboard2DFixed/Dynamic`、`BillboardMaterialCreateConfig`
6. 旧枚举：`GeometryMode::BillboardCameraFacing/AxisLocked`、`ShaderDataSchema::BillboardSizeUVec2`
7. 旧示例：`example/Basic/BillboardTest_use_ECS.cpp`（若 Step 6 改完后等价于死代码）
8. 同步：CMake、`*.vcxproj`、`VKString.cpp` 字符串映射、文档

---

## 1. 前置条件（**严格检查，否则停止**）

- [ ] Step 6 完成：`git grep` 5 份清单确认除白名单外 0 命中。
- [ ] **序列化兼容已就位**：`Sprite2DComponent::DeserializeFromRecord` 中存在 `"Quad"` → Sprite2D 的转换分支。
- [ ] 已对 `feature/sprite2d` 分支打 tag：`git tag sprite2d-step6-done`，便于回滚。
- [ ] 已询问/通知所有协作者本次删除的破坏性。

---

## 2. 删除顺序（叶子 → 根）

每完成一组立刻：`cmake configure → 全量构建 → 跑全部保留示例 → commit`。

### 2.1 第一波：旧示例 + 旧 Group（外层叶子）

```pwsh
git rm example/Basic/BillboardTest_use_ECS.cpp        # 若 Step 6 后已无价值
git rm -r inc/hgl/ecs/support/billboard
git rm -r src/ecs/support/billboard
```

同步：

- `src/ecs/CMakeLists.txt`：删除 `ECS_SUPPORT_BILLBOARD_FILES` 相关 set / append / source_group 三段
- `src/ecs/core/DefaultSystems.cpp`：删除 `InstallBillboardGroup`、`reg.RegisterGroupInstaller("Billboard", ...)`
- `example/Basic/CMakeLists.txt`：删除对应 add_basic_example 行

构建 + 跑示例 + commit：`step7: drop BillboardRenderPipelineGroup`。

### 2.2 第二波：旧 Quad 系统

```pwsh
git rm inc/hgl/ecs/systems/render/QuadResourcePrepareSystem.h
git rm src/ecs/systems/render/QuadResourcePrepareSystem.cpp
git rm inc/hgl/ecs/systems/render/QuadMaterialBindingSystem.h
git rm src/ecs/systems/render/QuadMaterialBindingSystem.cpp
```

同步：

- `src/ecs/CMakeLists.txt` 同上
- 任何 `#include` 残留（理论上 Step 6 已清干净，再 grep 一次保险）

构建 + 跑示例 + commit。

### 2.3 第三波：QuadComponent

```pwsh
git rm inc/hgl/ecs/components/QuadComponent.h
git rm src/ecs/components/QuadComponent.cpp
```

同步：

- 序列化 dispatch 表（如 `ComponentSerializerRegistry`）里删 `"Quad"` 注册
  - **保留** `Sprite2DComponent::DeserializeFromRecord` 内部的 legacy `"Quad"` 转换分支（一次性兼容旧资产）
- CMake / vcxproj

构建 + 跑示例 + commit：`step7: drop QuadComponent (Sprite2D legacy deserializer kept)`。

### 2.4 第四波：旧 ShaderGen + GLSL

```pwsh
git rm src/ShaderGen/3d/M_BillboardFixedSize.cpp
git rm src/ShaderGen/3d/M_BillboardDynamicSize.cpp
git rm ShaderLibrary/compositor/main_forward_billboard_fixed.vert.glsl
git rm ShaderLibrary/compositor/main_forward_billboard_dynamic.vert.glsl
```

同步：

- `src/ShaderGen/CMakeLists.txt`
- `src/ShaderGen/3d/Build3DCommon.{h,cpp}`：删除 `MakeBillboardKeyBase`
- 删除 `inc/hgl/mtl/BillboardMaterialCreateConfig.h`
- 删除 `CreateBillboard2DFixed/Dynamic` 声明 + 实现

构建 ShaderGen + 全工程 + commit。

### 2.5 第五波：枚举与 schema

`inc/hgl/mtl/MaterialVariantKey.h`：

```diff
 enum class GeometryMode : uint8_t
 {
     Mesh3D = 0,
     Quad2D,
     ScreenRect,
-    BillboardCameraFacing,
-    BillboardAxisLocked,
     Sprite2DCameraFacing,
     Sprite2DAxisLocked,
     ENUM_END
 };
```

> ⚠️ 删除会让 `Sprite2DCameraFacing` 等枚举值的 underlying int 改变 → **所有缓存的 SPIR-V 必须清空重生**。
>
> 解决：删除 `build/shader_cache/` 整个目录，重跑 ShaderGen。

`inc/hgl/graph/mtl/ShaderDataBlock.h`：

```diff
-    BillboardSizeUVec2,
     Sprite2DTransform,
```

`src/SceneGraph/Vulkan/VKString.cpp`：删除 `BillboardCameraFacing/AxisLocked` / `BillboardSizeUVec2` 字符串映射。

构建 + 清缓存 + 重跑 ShaderGen + 跑全部示例 + commit。

### 2.6 第六波：旧 vcxproj / 文档

- `*.vcxproj` / `*.vcxproj.filters`：grep 一遍，删除已不存在的文件条目（Visual Studio 有时不自动同步）。
- `doc/PipelineMaterialBatch_架构拆分说明.md`：把 Billboard 字样改为 Sprite2D。
- `doc/ECS_响应式参与系统设计.md`：同上。
- `README.md` / `CHANGELOG.md`：写一段 breaking change 说明。

---

## 3. 验证

每一波删除完成后：

- [ ] `cmake -S . -B build` 0 error / 0 warning。
- [ ] `cmake --build build --config Debug` 0 error。
- [ ] `git grep -i "billboard" -- ":!doc/*"` 命中数严格递减；最终应为 0（除 CHANGELOG 历史描述）。
- [ ] `git grep -i "QuadComponent" -- ":!doc/*"` 命中 0。
- [ ] 跑所有保留示例：
  - `01_Sprite2D`（原 01_Billboard 改名后）
  - `02_Sprite2DECS`
  - `03_Sprite2DPerspectiveECS`
  - `05_FacingMeshBillboardECS`（如保留）
  - 其他可能存在的非 Sprite2D 示例
  - 全部视觉与 Step 5 基线 / Step 6 基线一致。
- [ ] RenderDoc 抓帧：camera UBO 非零、mi_data 32B stride、validation 0 报错。

### 3.1 序列化兼容回归

把 Step 5 之前用旧 `QuadComponent` 序列化的资产（如 `*.scene` / `*.entities`）放进来，确认能用新的 `Sprite2DComponent::DeserializeFromRecord` 读出，且渲染正常。

> 没有旧资产可以测的话，去 git 历史找一份，临时 deserialize 一次，验证完即可。

---

## 4. 常见坑

- ❌ **SPIR-V 缓存未清** → 删除 `BillboardCameraFacing` 后枚举值偏移，旧 `Sprite2DCameraFacing.spv` 文件名/hash 与新枚举不匹配，运行时找不到 variant。**必须清 `build/shader_cache/`**。
- ❌ **vcxproj 残留旧文件** → VS 报 `C1083: cannot open ... QuadComponent.cpp`。手工编辑 vcxproj 或重跑 cmake configure。
- ❌ **序列化未兼容就删 QuadComponent** → 旧资产打开崩溃。先在 §1 prerequisites 严格 check。
- ❌ **删除时跨好几波合一个 commit** → 中间出问题难二分。严格按 6 波，每波一个 commit。
- ❌ `BillboardMaterialCreateConfig` 还被某个未识别角落引用 → grep 没找到是因为 typedef / using alias。删之前 `git grep -n "BillboardMaterial"` 双重确认。

---

## 5. 回滚方案

```pwsh
# 整步回滚到 Step 6 末尾
git reset --hard sprite2d-step6-done
# 清 SPIR-V 缓存
Remove-Item -Recurse -Force build/shader_cache
```

---

## 6. Step 7 通关条件

- [ ] 6 波删除全部完成，分别 commit。
- [ ] `git grep -i "billboard" -- ":!doc/Sprite2D_*.md" ":!CHANGELOG.md"` 命中 0。
- [ ] `git grep -in "Quad(Component|ResourcePrepare|MaterialBinding)" -- ":!doc/Sprite2D_*.md"` 命中 0。
- [ ] 全部保留示例视觉零退化。
- [ ] 旧资产能通过 legacy deserializer 读取并渲染正确。
- [ ] SPIR-V cache 已清并重生；`*.spv` 文件名只剩 Sprite2D 系列。
- [ ] 本 .md 末尾追加 `已通过：YYYY-MM-DD by xxx`。
