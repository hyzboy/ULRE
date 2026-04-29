# Billboard / Quad → Sprite2D 统一迁移方案

> 状态：草案 v1
> 分支：`refactor/shader_struct_optimize`
> 影响范围：ShaderGen（Billboard 两份）、ECS（Quad 资源/绑定系统、`QuadComponent`、`BillboardRenderPipelineGroup`）、示例 `03_BillboardPerspectiveECS` 等所有使用 Billboard/Quad 的代码。

---

## 0. 目标

- 完全废弃当前 “1 顶点 → 4 角矩形” 的 Billboard 几何展开方式。
- 用一个**中心在 (0,0)、边长为 1×1 的内置 2D 正方形 mesh** 作为绘制几何，所有 sprite 走顶点变换得到屏幕矩形。
- 上层 API 改为传入 **pivot + size**（以及可扩展的 rotation/tint），不再区分 Billboard 与 Quad，统一称为 **Sprite2D**。
- 仍然兼容已有的 `Texture2DArray` 域批处理与 PrimitiveCollect → PipelineMaterialBatch 流水线。
- 旧符号（`QuadComponent`、`BillboardRenderPipelineGroup`、`CreateBillboard2DFixed/Dynamic`）整体下线，**不保留向后兼容别名**——一次性改干净。

---

## 1. 现状回顾（要替换掉的关键点）

| 模块 | 现状 | 需要改成 |
|---|---|---|
| `src/ShaderGen/3d/M_BillboardFixedSize.cpp`<br>`src/ShaderGen/3d/M_BillboardDynamicSize.cpp` | 顶点流 = 单个 `vec3 Position`；`GeometryMode::BillboardAxisLocked` / `BillboardCameraFacing` 在 shader 中将 1 点扩成 4 个角；尺寸来自 `ShaderDataSchema::BillboardSizeUVec2` | 单一 `M_Sprite2D.cpp`（按需保留 fixed / dynamic 两个 variant），顶点流 = `vec2 Position + vec2 TexCoord`；新 `GeometryMode::Sprite2DCameraFacing` / `Sprite2DAxisLocked`；逐顶点变换 |
| `inc/hgl/mtl/MaterialVariantKey.h` | `GeometryMode { Mesh3D, Quad2D, ScreenRect, BillboardCameraFacing, BillboardAxisLocked }` | 删除两个 `Billboard*`，新增 `Sprite2DCameraFacing`、`Sprite2DAxisLocked` |
| `inc/hgl/graph/mtl/ShaderDataBlock.h`（`ShaderDataSchema`） | `BillboardSizeUVec2` | 删除；新增 `Sprite2DTransform`（size + pivot + rotation + tint + flags） |
| `inc/hgl/ecs/components/QuadComponent.h` | `QuadComponent`：`pixel_size`/`world_size` + `front_face` + `texture_path` | 删除；新增 `Sprite2DComponent`：`pivot`、`size`(像素 or 世界)、`rotation`、`tint`、`mesh*`(默认内置 unit‑square)、`front_face`、`texture_path`、`domain_tag` |
| `inc/hgl/ecs/systems/render/QuadResourcePrepareSystem.h/.cpp` | 维护一个全局 `shared_primitive`（单点）+ 域纹理数组 | 改名 `Sprite2DResourcePrepareSystem`，维护 **内置 unit‑square primitive**（共享）+ 域纹理数组 |
| `inc/hgl/ecs/systems/render/QuadMaterialBindingSystem.h/.cpp` | 给 `QuadComponent` 绑定 shared_primitive + per‑MI 数据（尺寸） | 改名 `Sprite2DMaterialBindingSystem`，给 `Sprite2DComponent` 绑定共享 unit‑square primitive + per‑MI 数据（pivot/size/rotation/tint） |
| `inc/hgl/ecs/support/billboard/BillboardRenderPipelineGroup.h/.cpp` | 名为 Billboard | 改名 `Sprite2DRenderPipelineGroup`，目录搬到 `inc/hgl/ecs/support/sprite2d/`，`src` 同步 |
| `example/Basic/BillboardTest_use_ECS.cpp` | 使用 `QuadComponent` | 改名 `Sprite2DTest_use_ECS.cpp`（即 `03_Sprite2DPerspectiveECS`），改用 `Sprite2DComponent` |

---

## 2. 新数据结构

### 2.1 内置 2D Mesh（无须 Sprite2DMesh 资源类）

由 `Sprite2DResourcePrepareSystem` 在 `EnsureSharedResources()` 中一次性建立：

- 顶点（4 个）：
  ```
  (-0.5, -0.5, 0,0)
  ( 0.5, -0.5, 1,0)
  ( 0.5,  0.5, 1,1)
  (-0.5,  0.5, 0,1)
  ```
- 索引（6 个）：`0,1,2, 0,2,3`
- 顶点格式：
  ```cpp
  constexpr FixedVertexEntry SPRITE2D_VERTEX[] = {
      { VAT_VEC2, VAN::Position },
      { VAT_VEC2, VAN::TexCoord },
  };
  ```
- 命名：`shared_unit_square_primitive`（仍是 `graph::Primitive*`）。

> 之所以不引入 `Sprite2DMesh` 资源类：本次目标只是“中心 (0,0)、1×1 的正方形”，不做任意 2D 模型。后续若要扩展多边形/glyph，只需把 `shared_unit_square_primitive` 字段升级为 `unordered_map<mesh_id, Primitive*>`，无需再改 shader / per‑MI 数据布局。

### 2.2 `Sprite2DTransform` per‑instance schema

替换原 `BillboardSizeUVec2`：

```
struct Sprite2DTransform
{
    vec2  size;        // fixed 模式 = 像素；dynamic 模式 = 世界单位
    vec2  pivot;       // 默认 (0.5, 0.5)，相对于 mesh 局部坐标
    float rotation;    // 弧度，绕屏幕法线
    uint  tint_rgba8;  // packUnorm4x8
    uint  flags;       // bit0 = fixed_size, bit1 = axis_locked, ...
    uint  _pad;
};
```

挂在 `MaterialBindingInstanceData` SSBO 里，per‑MI 一份；纹理仍由 `MaterialBindingInstanceTexture` SSBO 选层（domain 路径）。

### 2.3 `Sprite2DComponent`

```cpp
class Sprite2DComponent : public PrimitiveComponent
{
private:
    bool                fixed_size = true;       // pixel or world
    hgl::math::Vector2u pixel_size{256, 256};
    glm::vec2           world_size{1.f, 1.f};
    glm::vec2           pivot{0.5f, 0.5f};       // mesh 局部坐标，(0.5,0.5)=居中
    float               rotation = 0.0f;         // 弧度
    glm::u8vec4         tint{255, 255, 255, 255};
    VkFrontFace         front_face = VK_FRONT_FACE_CLOCKWISE;

    hgl::OSString       texture_path;
    hgl::OSString       applied_texture;
    bool                texture_dirty = false;
    std::string         domain_tag;              // 空 = legacy 单纹理路径
    graph::Texture2D*   texture = nullptr;
    graph::Sampler*     sampler = nullptr;
public:
    // setter/getter 按字段对齐 QuadComponent 命名风格
};
```

> `pivot` 用 `glm::vec2`，便于以后允许 mesh 不是中心 (0,0) 时（例如锚在底部的人物 sprite），仍可灵活控制。

---

## 3. Shader 变换公式

逐顶点（在新的 `M_Sprite2D.cpp` 生成的 VS 里）：

```glsl
// per-instance (从 SSBO 读入)
vec2  size     = inst.size;
vec2  pivot    = inst.pivot;
float rot      = inst.rotation;
vec4  tint     = unpackUnorm4x8(inst.tint_rgba8);
bool  fixed    = (inst.flags & 1u) != 0u;
bool  axisLock = (inst.flags & 2u) != 0u;

// per-vertex
vec2 local = (in_position - pivot) * size;     // 应用 pivot + 尺寸
float c = cos(rot), s = sin(rot);
vec2 r  = vec2(c*local.x - s*local.y,
               s*local.x + c*local.y);

vec3 anchor = transform.position;              // 来自 TransformData
vec3 world;
if (axisLock) {
    // BillboardAxisLocked：仍朝相机，但 size 视作像素 → 转 NDC 后偏移 clip space
    vec4 clip = camera.viewProj * vec4(anchor, 1.0);
    vec2 ndcOffset = r / viewport.size * 2.0 * clip.w; // size 已是像素
    gl_Position = clip + vec4(ndcOffset, 0, 0);
} else {
    // CameraFacing：size = 世界单位
    world = anchor + camera.right * r.x + camera.up * r.y;
    gl_Position = camera.viewProj * vec4(world, 1.0);
}

out_uv    = in_texcoord;
out_color = tint;
```

> 当 `fixed == true` 但又不锁屏时（混合模式），按需扩展；初版可只支持两种组合：`fixed_size && axis_locked` ↔ Sprite2DAxisLocked，`!fixed_size && !axis_locked` ↔ Sprite2DCameraFacing，两个 variant 各一份。

---

## 4. 文件变更清单（一次性、不保留旧名）

### 4.1 删除

- `src/ShaderGen/3d/M_BillboardFixedSize.cpp`
- `src/ShaderGen/3d/M_BillboardDynamicSize.cpp`
- `inc/hgl/ecs/components/QuadComponent.h` + `src/.../QuadComponent.cpp`
- `inc/hgl/ecs/systems/render/QuadResourcePrepareSystem.h` + 实现
- `inc/hgl/ecs/systems/render/QuadMaterialBindingSystem.h` + 实现
- `inc/hgl/ecs/support/billboard/BillboardRenderPipelineGroup.h` + 实现
- 枚举值 `GeometryMode::BillboardCameraFacing`、`BillboardAxisLocked`
- `ShaderDataSchema::BillboardSizeUVec2`
- 工厂函数 `CreateBillboard2DFixed`、`CreateBillboard2DDynamic`
- 旧示例 `example/Basic/BillboardTest_use_ECS.cpp`

### 4.2 新增

- `src/ShaderGen/3d/M_Sprite2D.cpp`
  - 导出 `CreateSprite2DCameraFacing(profile, cfg)` 与 `CreateSprite2DAxisLocked(profile, cfg)`，或合并为 `CreateSprite2D(profile, cfg)`，由 `cfg->axis_locked` 选 variant。
- `inc/hgl/mtl/Sprite2DMaterialCreateConfig.h`（替代 `BillboardMaterialCreateConfig`，保留 `blend_mode`、`use_texture_array`、`base_color_channel`，新增 `axis_locked`、`fixed_size`）。
- `inc/hgl/ecs/components/Sprite2DComponent.h` + `src/.../Sprite2DComponent.cpp`
- `inc/hgl/ecs/systems/render/Sprite2DResourcePrepareSystem.h` + 实现
- `inc/hgl/ecs/systems/render/Sprite2DMaterialBindingSystem.h` + 实现
- `inc/hgl/ecs/support/sprite2d/Sprite2DRenderPipelineGroup.h` + `src/.../Sprite2DRenderPipelineGroup.cpp`
- `example/Basic/Sprite2DTest_use_ECS.cpp`（即 `03_Sprite2DPerspectiveECS`）

### 4.3 修改

- `inc/hgl/mtl/MaterialVariantKey.h` —— 替换 `GeometryMode` 两个值。
- `inc/hgl/graph/mtl/ShaderDataBlock.h` —— 替换 schema。
- `src/ShaderGen/3d/Build3DCommon.h/.cpp` —— `MakeBillboardKeyBase` → `MakeSprite2DKeyBase`，并修正 `geometry_mode` 默认值。
- `src/SceneGraph/Vulkan/VKString.cpp` —— `GeometryMode` 字符串映射。
- `src/InlineGeometry/GeometryValidator.h`、`AxisBoundingBoxSquareArray.cpp` 等出现旧枚举的地方。
- `src/SceneGraph/render/PipelineMaterialBatch.cpp`、`PipelineMaterialRenderer.cpp` 中 Billboard/Quad 相关分支。
- `inc/hgl/graph/RenderFramework.h` 内的注册/调度引用。
- 任何 CMake / 工程文件（`*.vcxproj` / `CMakeLists.txt`）需要同步删除/添加上述 `.cpp / .h`。
- 文档：`doc/PipelineMaterialBatch_架构拆分说明.md`、`doc/ECS_响应式参与系统设计.md` 中的 Billboard 字样统一改写为 Sprite2D。

---

## 5. 迁移步骤（建议顺序，每一步可独立编译通过）

1. **新增着色器与 schema（不删旧的）**
   - 在 `MaterialVariantKey.h` 增加 `Sprite2DCameraFacing` / `Sprite2DAxisLocked`（仍保留 Billboard 两个值，避免一次性失败）。
   - 在 `ShaderDataBlock.h` 增加 `Sprite2DTransform`。
   - 新增 `M_Sprite2D.cpp`，注册 variant 并实现新的 VS 代码。
   - 跑一遍 ShaderGen，确认新 variant 能编译成功。

2. **新增内置 unit‑square primitive**
   - 在新 `Sprite2DResourcePrepareSystem`（先复制一份 `QuadResourcePrepareSystem` 改名）里实现 `EnsureSharedResources()`，构建 4 顶点/6 索引正方形，引用新 ShaderMaterialProgram。

3. **新增 `Sprite2DComponent` + 绑定系统**
   - 复制 `QuadComponent` → `Sprite2DComponent`，加入 `pivot`、`rotation`、`tint`，移除 `pixel_size`/`world_size` 之外的 quad 专有字段。
   - 复制 `QuadMaterialBindingSystem` → `Sprite2DMaterialBindingSystem`，per‑MI 数据写入 `Sprite2DTransform`。

4. **新增 `Sprite2DRenderPipelineGroup`**
   - 注册上面两个系统。

5. **新增示例 `Sprite2DTest_use_ECS.cpp`**
   - 用 `Sprite2DComponent` + `Sprite2DRenderPipelineGroup` 重写老 BillboardTest 的所有用例，验证：
     - `axis_locked` + `fixed_size`：固定像素朝相机
     - `!axis_locked` + `!fixed_size`：世界尺寸朝相机
     - 非中心 pivot
     - 旋转
     - 多 sprite + Texture2DArray 域批处理
   - 跑通后此示例即作为新基线。

6. **逐步替换调用点**
   - 全工程 grep `QuadComponent` / `BillboardRenderPipelineGroup` / `CreateBillboard2D` / `BillboardSizeUVec2` / `GeometryMode::Billboard*`，改为 Sprite2D 版本。
   - 每改一个调用点跑一次构建。

7. **删除旧实现**
   - 删除 §4.1 列出的所有文件 / 枚举 / schema / 工厂函数。
   - 修正 CMake、`*.vcxproj`、文档、`VKString.cpp` 字符串映射。
   - 全量构建 + 跑示例回归。

8. **收尾**
   - 更新 `doc/PipelineMaterialBatch_架构拆分说明.md` 中的术语。
   - 写一段 CHANGELOG / commit message：`Refactor: drop Billboard/Quad, unify under Sprite2D (unit-square mesh + pivot/size)`。

---

## 6. 兼容性与影响评估

- **不保留向后兼容别名**（按需求一次性改干净）。任何外部使用 `QuadComponent` / `BillboardRenderPipelineGroup` 的代码都需要适配；这是一次破坏性改动，应在 commit message / PR 描述里明确指出。
- **序列化兼容性**：`QuadComponent::SerializeToRecord` / `DeserializeFromRecord` 中的 `GetSerializationType()` 字符串若被资产引用，需要在 `Sprite2DComponent` 中提供新字符串，并在反序列化路径里加一个一次性转换分支（旧 `"Quad"` → 转为 `"Sprite2D"` 默认参数：`pivot=(0.5,0.5)`、`rotation=0`、`tint=white`），完成后即可移除。否则旧场景文件无法加载。
- **Drawcall 数量不变**：仍然是 1 drawcall / domain（共用 unit‑square + Texture2DArray），与现状持平。
- **后续扩展点**：`shared_unit_square_primitive` 升级成 `unordered_map<mesh_id, Primitive*>` 即可支持任意 2D 模型，shader / per‑MI 数据无需再变。

---

## 7. 验收清单

- [ ] `M_Sprite2D` 两个 variant 在 ShaderGen 中成功生成。
- [ ] `Sprite2DResourcePrepareSystem` 创建 unit‑square primitive 并被 `Sprite2DRenderPipelineGroup` 注册。
- [ ] 示例 `03_Sprite2DPerspectiveECS` 复现原 BillboardTest 的全部行为。
- [ ] 旋转 / 非中心 pivot / tint 三项新能力在示例中可见。
- [ ] 多 sprite + 同 domain 仍是 1 drawcall（用 RenderDoc 抓帧确认）。
- [ ] 全工程构建无 Billboard/Quad 残留符号引用。
- [ ] 文档与字符串映射更新到位。

---

## 8. 风险与备注

- **Pivot 与 TransformComponent 的关系**：`pivot` 只影响 mesh 局部偏移，不会改变实体的世界坐标 anchor；旋转中心 = `(world_anchor) + (pivot 在屏幕平面内的偏移)`。需要在示例与文档中明确，避免与 `TransformComponent` 的层级 pivot 混淆。
- **AxisLocked 像素模式**的 NDC 偏移公式依赖 `viewport.size`，必须确保 `ViewportInfo` UBO 已被新 variant 列入 `UBOSemanticSet`（沿用 `BILLBOARD_*_BASE_UBOS` 即可）。
- **Front face**：unit‑square mesh 的索引顺序固定为 CCW（`0,1,2,0,2,3`），`Sprite2DComponent::front_face` 仅控制管线的 `VkFrontFace`，不再需要在 shader 里翻转角点顺序——这是相对旧实现的简化。
- **序列化迁移**应在删除 `QuadComponent` 之前完成，否则旧资产无法读取。
