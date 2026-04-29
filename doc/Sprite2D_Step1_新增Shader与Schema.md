# Sprite2D 迁移 — Step 1：新增 Shader 与 Schema（保留旧实现）

> 状态：可独立编译、可独立测试
> 风险等级：低
> 预计耗时：3–4 小时
> 关键原则：**只增不删**。Billboard 旧 variant 必须仍然能跑通 03_BillboardPerspectiveECS。

---

## 0. 目标

1. 在枚举与 schema 层面引入 `Sprite2D` 概念，**与旧 Billboard 共存**。
2. 新增 `M_Sprite2D.cpp`，注册两个新 variant：`Sprite2DCameraFacing`、`Sprite2DAxisLocked`。
3. 通过 ShaderGen 产出对应 SPIR-V，确认编译通过；尚不接入任何 ECS 路径。
4. 旧 `M_BillboardFixedSize / M_BillboardDynamicSize` 保持原样。

---

## 1. 前置条件

- 已切到分支 `feature/sprite2d`（或同等隔离分支）。
- 旧 `03_BillboardPerspectiveECS` 在主基线下渲染正常（图片正向、有透视、无 Validation 报错），作为 baseline。
- `cmake --build build --target ShaderGen` 可独立跑。

> 后续每一步都要保证这条 baseline 仍可跑。

---

## 2. 涉及文件

### 2.1 枚举 / Schema

| 路径 | 改动 |
|---|---|
| `inc/hgl/mtl/MaterialVariantKey.h` | **追加**枚举值，不动旧值 |
| `inc/hgl/graph/mtl/ShaderDataBlock.h` | **追加** `Sprite2DTransform` schema，不动旧值 |
| `src/SceneGraph/Vulkan/VKString.cpp` | 为新枚举值补充字符串映射（`ToString` / 反查） |

### 2.2 Shader 生成

| 路径 | 改动 |
|---|---|
| `src/ShaderGen/3d/M_Sprite2D.cpp` | **新建** |
| `src/ShaderGen/3d/Build3DCommon.h` | **追加** `MakeSprite2DKeyBase()`，不动 `MakeBillboardKeyBase()` |
| `src/ShaderGen/3d/Build3DCommon.cpp` | 同上 |
| `src/ShaderGen/CMakeLists.txt`（或子 CMake） | 把 `M_Sprite2D.cpp` 加进编译列表 |

### 2.3 配置入口（仅声明，先不让用）

| 路径 | 改动 |
|---|---|
| `inc/hgl/mtl/Sprite2DMaterialCreateConfig.h` | **新建**，结构体先定义出来 |

> Step 1 不创建对应 ECS Component / 系统，也不创建示例。这些放到 Step 3、Step 5。

---

## 3. 执行步骤

### 3.1 增加枚举值（不要碰旧值）

`inc/hgl/mtl/MaterialVariantKey.h`：

```cpp
enum class GeometryMode : uint8_t
{
    Mesh3D = 0,
    Quad2D,
    ScreenRect,
    BillboardCameraFacing,   // 旧（保留）
    BillboardAxisLocked,     // 旧（保留）
    Sprite2DCameraFacing,    // 新增 ← 顺序追加在末尾，不要插到中间
    Sprite2DAxisLocked,      // 新增

    ENUM_END
};
```

> ⚠️ **不要把新枚举值插到中间**。`GeometryMode` 会进入 variant hash，如果在中间插入会让所有已有 variant 的 hash 整体偏移，导致旧的 SPIR-V 缓存全部失效。一律 append 到末尾。

`inc/hgl/graph/mtl/ShaderDataBlock.h`：

```cpp
enum class ShaderDataSchema : uint8_t
{
    // ... 旧值保留 ...
    BillboardSizeUVec2,      // 旧（保留）
    Sprite2DTransform,       // 新增（末尾追加）
    ENUM_END
};
```

对应 schema 描述（在 `ShaderDataBlock.cpp` 或 schema 注册表中）：

```cpp
struct Sprite2DTransform
{
    vec2  size;        // 16B 对齐：单成员，仍占 16B
    vec2  pivot;
    float rotation;
    uint  tint_rgba8;  // packUnorm4x8
    uint  flags;       // bit0 = fixed_size, bit1 = axis_locked
    uint  _pad0;
};
// 总大小 32B，符合 std140 vec4 对齐
```

### 3.2 字符串映射

`src/SceneGraph/Vulkan/VKString.cpp` 中找到 `ToString(GeometryMode)` 与反查函数（如 `FromString`），追加：

```cpp
case GeometryMode::Sprite2DCameraFacing: return "Sprite2DCameraFacing";
case GeometryMode::Sprite2DAxisLocked:   return "Sprite2DAxisLocked";
```

> 反查路径同步加。这是后续 ShaderGen 输出文件名的来源，不加会导致 `M_Sprite2D` 注册失败。

### 3.3 新建 `Sprite2DMaterialCreateConfig.h`

```cpp
// inc/hgl/mtl/Sprite2DMaterialCreateConfig.h
#pragma once
#include<hgl/mtl/MaterialCreateConfig.h>

namespace hgl::graph::mtl
{
    struct Sprite2DMaterialCreateConfig : public MaterialCreateConfig
    {
        bool                axis_locked    = false;  // true = 屏幕固定像素
        bool                fixed_size     = false;  // true = pixel_size 解释为像素
        bool                use_texture_array = false;
        TextureChannelHint  base_color_channel = TextureChannelHint::RGBA;
        RenderAlphaMode     blend_mode = RenderAlphaMode::Opaque;
    };
}
```

> 这里只是把字段先放出来，Step 1 不注册 creator。

### 3.4 新建 `M_Sprite2D.cpp`

参考 `M_BillboardFixedSize.cpp` / `M_BillboardDynamicSize.cpp` 的写法，**整体结构复用**，重点替换：

- `geometry_mode` → `Sprite2DCameraFacing` / `Sprite2DAxisLocked`
- 顶点输入：`vec2 Position + vec2 TexCoord`（不再是 `vec3 Position` 单点扩展）
- per-instance schema → `Sprite2DTransform`

VS 主体（伪代码，最终在 ShaderGen 模板里输出）：

```glsl
#version 450
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_texcoord;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

// camera / viewport / mbi_data 来自现有 UBO/SSBO 绑定

void main()
{
    Sprite2DTransform inst = mbi_data[mbi_id];

    vec2  size     = inst.size;
    vec2  pivot    = inst.pivot;
    float rot      = inst.rotation;
    vec4  tint     = unpackUnorm4x8(inst.tint_rgba8);
    bool  fixed_sz = (inst.flags & 1u) != 0u;
    bool  axisLock = (inst.flags & 2u) != 0u;

    vec2 local = (in_position - (pivot - vec2(0.5))) * size;
    float c = cos(rot), s = sin(rot);
    vec2 r  = vec2(c*local.x - s*local.y,
                   s*local.x + c*local.y);

    vec3 anchor = GetTransformPosition();

    if (axisLock) {
        vec4 clip = camera.vp * vec4(anchor, 1.0);
        // size 视作像素：先转 NDC offset
        vec2 ndcOffset = r / vec2(viewport.canvas_resolution) * 2.0 * clip.w;
        gl_Position = clip + vec4(ndcOffset, 0, 0);
    } else {
        vec3 world = anchor + camera.billboard_right * r.x + camera.billboard_up * r.y;
        gl_Position = camera.vp * vec4(world, 1.0);
    }

    out_uv    = in_texcoord;
    out_color = tint;
}
```

> ⚠️ Vulkan UV 约定：mesh 的 TexCoord 在 Step 2 中按 V=0 在顶部、V=1 在底部存。AxisLocked 由于使用 NDC 偏移，相对于世界 Y 是反向的；如果出现上下颠倒，可在 fixed-size variant 内做 `out_uv.y = 1.0 - in_texcoord.y;` 补偿。**这一点必须等到 Step 5 跑通示例后再回来确认**，不要在 Step 1 凭空加补偿。

### 3.5 接入 ShaderGen 注册表

参考 `Build3DCommon.cpp` 中 Billboard 相关入口，**新增**：

```cpp
mtl::MaterialVariantKey MakeSprite2DKeyBase(bool axis_locked)
{
    mtl::MaterialVariantKey k = MakeMesh3DKeyBase();
    k.geometry_mode = axis_locked
        ? mtl::GeometryMode::Sprite2DAxisLocked
        : mtl::GeometryMode::Sprite2DCameraFacing;
    k.vertex_attrib_bits = VAB(VertexAttrib::TexCoord);  // Position 默认在
    return k;
}
```

`M_Sprite2D.cpp` 中按 axis_locked 分别向 `MaterialVariantRegistry` 注册两个 variant，shader 路径：

- `compositor/main_forward_sprite2d_dynamic.vert.glsl`（CameraFacing）
- `compositor/main_forward_sprite2d_fixed.vert.glsl`（AxisLocked）
- 共用 `compositor/main_forward_billboard.frag.glsl`（贴图采样逻辑可复用，不用新建）

> 这两个 GLSL 模板文件本次也属于"新增"，放在 `ShaderLibrary/compositor/` 下；旧的 `main_forward_billboard_dynamic.vert.glsl` / `main_forward_billboard_fixed.vert.glsl` 完全不动。

---

## 4. 验证（必须全部通过才能进入 Step 2）

1. **编译**
   ```pwsh
   cmake --build build --target ULRE.ShaderGen --config Debug
   cmake --build build --config Debug
   ```
   不应出现 warning C4061（switch 漏 enum）。如果出现，去把缺的 enum 项补上 `default: assert(...);` 或显式 case。

2. **ShaderGen 跑通**
   - 启动 ShaderGen 离线模式，确认输出目录里出现 `Sprite2DCameraFacing.*.spv` / `Sprite2DAxisLocked.*.spv` 之类文件。
   - SPIR-V 反汇编（`spirv-dis`）检查：UBO 绑定 set/binding 与现有 Billboard 一致；per-instance schema 大小为 32 字节。

3. **旧 baseline 不退化**
   - 跑 `03_BillboardPerspectiveECS`：图像、透视、Validation 全部与 Step 0 一致。
   - 跑 `01_Billboard`：同上。

4. **变体 hash 确认**
   - 用 ShaderGen 调试输出（`EnumerateRecipeKeys` 之类）打印 Billboard 旧两个 variant 的 hash，与 Step 0 的截图比对必须**完全相同**（验证 enum append 没有破坏旧 hash）。

---

## 5. 常见坑

- ❌ 把新枚举插在 `BillboardCameraFacing` 前面 → Billboard 旧 variant hash 偏移、SPIR-V 缓存全部失效、示例黑屏。
- ❌ 忘记在 `VKString.cpp` 加字符串映射 → `MaterialLibrary` 注册时返回 nullptr，看似 ShaderGen 跑通实则没注册。
- ❌ `Sprite2DTransform` 总大小不是 16 的倍数 → SSBO 数组 stride 与 GLSL `std430` 不匹配，per-instance 数据会错位（表现为旋转/尺寸完全乱）。**尾部 `_pad0` 必须保留**。
- ❌ VS 里直接读 `tint_rgba8` 当 vec4 → 必须用 `unpackUnorm4x8`，否则会被解释为整数。

---

## 6. 回滚方案

只做了"追加"，回滚最简单：

```pwsh
git restore inc/hgl/mtl/MaterialVariantKey.h
git restore inc/hgl/graph/mtl/ShaderDataBlock.h
git restore src/SceneGraph/Vulkan/VKString.cpp
git restore src/ShaderGen/3d/Build3DCommon.h src/ShaderGen/3d/Build3DCommon.cpp
git rm src/ShaderGen/3d/M_Sprite2D.cpp
git rm inc/hgl/mtl/Sprite2DMaterialCreateConfig.h
git rm ShaderLibrary/compositor/main_forward_sprite2d_*.vert.glsl
```

完成后 ShaderGen 退回 Step 0 状态。

---

## 7. Step 1 通关条件（Done Definition）

- [ ] 全量构建 0 error 0 warning（除 `[[deprecated]]` 之类的预期 warning）。
- [ ] ShaderGen 输出包含两个新 variant 的 SPIR-V。
- [ ] `03_BillboardPerspectiveECS`、`01_Billboard` 视觉 + Validation 与 Step 0 一致。
- [ ] Billboard 旧两个 variant 的 hash 与 Step 0 完全一致。
- [ ] 文档：在本 .md 末尾加一行 `已通过：YYYY-MM-DD by xxx`。
