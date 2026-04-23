# ShaderLibrary — GLSL Source Layout

GLSL 文件由 C++ 端的 `CompositorAssembler` 在运行时拼接成完整的 VS/FS 字符串，
再交给 glslang 做真正的预处理（解析 `#include`）和编译。

---

## 目录规范

| 目录 | 用途 | 规则 |
|---|---|---|
| `util/` | **纯工具函数**，无控制宏依赖，可被任意 shader 复用 | 每个文件一组强相关函数 + include guard；**禁止** 在此读 UBO/SSBO |
| `common/` | **运行时强约定**：UBO/SSBO 声明、varying 接口、采样器获取 | 文件名与 C++ 端描述符语义对应；有 guard |
| `control/` | **宏常量集中定义**：alpha 模式编号、varying location 编号等 | 只有 `#define`，无可执行代码 |
| `surface/` | **材质表面函数**：必须实现 `EvalSurface(SurfaceInput) → SurfaceOutput` | 可 `#include "util/"` 中的工具，不可 `#include "compositor/"` |
| `compositor/` | **VS/FS 组装模板**：由 C++ 生成的 entry-point 文件 include 进来 | 文件只做组装，不写真实算法；complex logic → `util/` |
| `ToneMap/` | 后处理色调映射曲线 | 每个文件实现同签名 `ToneMapping(vec3)` |

---

## 命名规范

- include guard 统一用 `ULRE_<UPPER_DIRNAME>_<UPPER_FILENAME>_GLSL`  
  例：`util/dither.glsl` → `#ifndef ULRE_UTIL_DITHER_GLSL`
- `util/` 文件：只暴露纯函数，**不读任何全局 UBO/SSBO binding**
- `common/` UBO 文件：guard 统一 `UBO_<NAME>_GLSL`，SSBO 用 `SSBO_<NAME>_GLSL`

---

## 控制宏约定（C++ 端注入）

C++ `CompositorAssembler` 在 `#version` 后注入以下宏，**不要在 .glsl 文件里 #define 这些**：

| 宏 | 含义 |
|---|---|
| `HAS_POSITION / HAS_NORMAL / HAS_TANGENT / HAS_TEXCOORD / HAS_COLOR` | vertex attrib 启用标志 |
| `ENABLE_LIGHTING` | 走 EvalLighting 路径 |
| `ALPHA_MODE_MASKED / ALPHA_MODE_DITHER` | alpha 处理模式 |
| `NEEDS_SKY / NEEDS_CAMERA` | UBO 按需 include |
| `TEXTURE_ARRAY_MODE` | 采样器为 sampler2DArray |
| `SURFACE_TYPE` | 对应 C++ `SurfaceType` 枚举整数值 |

---

## 典型组装流程（Forward Opaque Lit）

```
C++ 生成：
  #version 450
  #define HAS_POSITION
  #define HAS_NORMAL
  #define HAS_TEXCOORD
  #define ENABLE_LIGHTING

glslang 展开：
  #include "compositor/vert_forward_ubo.glsl"   // camera + transform + MI
  #include "compositor/vert_forward_main.glsl"   // VS main()

  #include "compositor/frag_forward_ubo.glsl"    // 按需 camera/sky
  #include "common/skylight_simple.glsl"
  #include "common/lighting_pbr.glsl"
  #include "surface/standard_surface.glsl"       // EvalSurface()
  #include "compositor/frag_forward_main.glsl"   // FS main()
```
