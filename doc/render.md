# ULRE 渲染系统总览

本文档是 ULRE 渲染管线的快速参考索引，供 AI 及开发者一次性获取关键术语、工作原理与示例导航。

## 相关深度文档（/doc/ecs/）

| 文档 | 主题 |
|------|------|
| [ecs/ecs-core-components.md](ecs/ecs-core-components.md) | ECSContext / Entity / System / World / WorldScheduler 核心架构 |
| [ecs/render-chain-analysis.md](ecs/render-chain-analysis.md) | 渲染链完整分析（RenderCollect → Batch → Draw 全流程） |
| [ecs/render-pipeline-group.md](ecs/render-pipeline-group.md) | RenderPipelineGroup 体系及所有派生实现（Primitive/Line/Text/Billboard/Terrain） |
| [ecs/primitive-geometry-vdm.md](ecs/primitive-geometry-vdm.md) | Primitive 完整构成：Geometry / GeometryData / VertexDataManager |
| [ecs/material-materialinstance-ecs-analysis.md](ecs/material-materialinstance-ecs-analysis.md) | Material / MaterialInstance 在 ECS 中的完整数据流 |
| [ecs/transform-data-management-and-transfer.md](ecs/transform-data-management-and-transfer.md) | Transform 数据管理与 GPU 传递（TransformAssignmentBuffer） |
| [ecs/ecs_sub_world.md](ecs/ecs_sub_world.md) | SubWorld / SubWorldComponent 多世界嵌套设计 |

---

## 术语

| 术语 | 说明 | 头文件 |
|------|------|--------|
| `VertexAttribBuffer`（VAB） | 继承自 `VkBufferOwner` 的 VBO 管理类 | `inc/hgl/vk/VKVertexAttribBuffer.h` |
| `IndexBuffer`（IBO） | 继承自 `VkBufferOwner` 的索引缓冲管理类 | `inc/hgl/vk/VKIndexBuffer.h` |
| `GeometryDataBuffer` | 绘制时的简易 VBO/IBO 数据集合结构 | `inc/hgl/graph/mesh/GeometryDataBuffer.h` |
| `GeometryDrawRange` | 绘制时的 vertex/index 范围数据集合 | `inc/hgl/graph/mesh/GeometryDrawRange.h` |
| `Material` | 一套 shader 配置（材质） | `inc/hgl/vk/VKMaterial.h` |
| `MaterialInstance` | 材质的一份独立参数配置 | `inc/hgl/vk/VKMaterialInstance.h` |
| `Pipeline` | `VkPipeline` 封装 | `inc/hgl/vk/pipeline/VKPipeline.h` |
| `MaterialPipelineKey` | Material + Pipeline 的复合索引键，用于渲染项分组 | `inc/hgl/ecs/core/MaterialPipelineKey.h` |
| `Primitive` | 单一渲染元素（Geometry + MaterialInstance + DrawRange） | `inc/hgl/graph/mesh/Primitive.h` |
| `StaticMesh` | 静态模型（多 Primitive/Material/MI 组合，**尚未完成**） | — |

---

## Material / MaterialInstance 数据工作原理

`Material` 内部维护一块 `mi_data_manager` 数据区，管理所有 `MaterialInstance` 的独有数据。  
`MaterialInstance` 创建时得到一个序列号，该序列号同时作为其在 `mi_data_manager` 中的槽位索引。

详见：[ecs/material-materialinstance-ecs-analysis.md](ecs/material-materialinstance-ecs-analysis.md)

---

## 关键工作类

### TransformAssignmentBuffer
提供 mat4x4 Transform ID 及真实矩阵数据的管理。  
- ID 以 **Instance Rate VAB** 形式传递给 Vertex Shader  
- 真实矩阵数据以 **SSBO/UBO** 形式传递  
- Shader 内通过 ID 取得对应 Transform Matrix  

详见：[ecs/transform-data-management-and-transfer.md](ecs/transform-data-management-and-transfer.md)

### MaterialInstanceAssignmentBuffer
提供 MaterialInstance ID 及真实参数数据的管理，机制与 TransformAssignmentBuffer 相同。

### VertexDataManager（VDM）
大 VBO/IBO 支持类，将所有顶点与索引数据集中管理于同一块 GPU 缓冲区。  
与 TransformAssignmentBuffer + MaterialInstanceAssignmentBuffer + Texture Array 配合，  
通过 Indirect Draw Buffer 实现**单 DrawCall 绘制同类所有 Mesh** 的终极合批目标。

详见：[ecs/primitive-geometry-vdm.md](ecs/primitive-geometry-vdm.md)

---

## 示例总览

### 基础示例（example/Basic/）

| 文件 | 窗口标题 | 说明 |
|------|----------|------|
| [draw_triangle.cpp](../example/Basic/draw_triangle.cpp) | — | 最原始三角形绘制，**不使用 VDM**，直接操作 VAB |
| [clock.cpp](../example/Basic/clock.cpp) | — | 时钟，ECS **动静分离**测试：12 刻度（静态）+ 3 指针（动态），共享同一三角形 Primitive |
| [auto_instance.cpp](../example/Basic/auto_instance.cpp) | — | 多三角形 ECS + RenderCollector **自动 Instance 合批**：相同 Material+Pipeline 的实体自动合并 |
| [auto_merge_material_instance.cpp](../example/Basic/auto_merge_material_instance.cpp) | — | 同一 Material 下多个不同颜色 **MaterialInstance** 批量渲染，演示 `MaterialInstanceAssignmentBuffer` 去重索引 |
| [SimpleCube.cpp](../example/Basic/SimpleCube.cpp) | — | ECS 立方体 + CameraSystem **ViewModel** 控制模式 |
| [RenderBoundBox.cpp](../example/Basic/RenderBoundBox.cpp) | — | ECS 多实体 + **AABB 包围盒**线框叠加渲染 |
| [RecursiveCube.cpp](../example/Basic/RecursiveCube.cpp) | Recursive Cube (ECS) | 递归立方体：从原点沿6面法线方向各展开10层，每层缩放0.9x，演示 Transform 父子链 |
| [RenderToTexture.cpp](../example/Basic/RenderToTexture.cpp) | Render To Texture (ECS) | **离屏渲染（RTT）**：两个 ECSContext，主场景与离屏 RT 分离，演示 `SetResourceNamePrefix` 资源追踪 |
| [06b_BasicLitMeshesECS.cpp](../example/Basic/06b_BasicLitMeshesECS.cpp) | BasicLit Meshes ECS | **BasicLit PBR 材质**（砖墙 Albedo+Normal+Roughness 贴图）ECS 多 Mesh 场景 |
| [06c_TextureBlinnPhongMeshesECS.cpp](../example/Basic/06c_TextureBlinnPhongMeshesECS.cpp) | TextureBlinnPhong Meshes ECS | **BlinnPhong 纹理材质** ECS 多 Mesh 场景，与 06b 结构类似，光照模型不同 |
| [PBRSpheresECS.cpp](../example/Basic/PBRSpheresECS.cpp) | PBR Spheres 10x10 (ECS) | **10×10 PBR 球体矩阵**：X 轴 Metallic(0→1)，Y 轴 Roughness(0.05→1)，经典 PBR 对比图 |
| [BillboardECS.cpp](../example/Basic/BillboardECS.cpp) | — | **告示牌**（解耦架构）：QuadComponent（几何+渲染）+ FacingTransformComponent（朝向相机旋转）|
| [BillboardPerspectiveECS.cpp](../example/Basic/BillboardPerspectiveECS.cpp) | — | **透视缩放告示牌**：世界空间大小，近大远小（`SetFixedPixelSize(false)`）|
| [FacingMeshBillboardECS.cpp](../example/Basic/FacingMeshBillboardECS.cpp) | — | **3D 网格朝向相机**（非 Quad Sprite，真实 3D 几何体 Billboard）|
| [FacingMeshBillboardZECS.cpp](../example/Basic/FacingMeshBillboardZECS.cpp) | — | **3D 网格朝向相机，Z-up 锁定**：BillboardZ 模式，无滚转，相机抬高时方向稳定 |

### Gizmo 示例（example/Gizmo/）

| 文件 | 窗口标题 | 说明 |
|------|----------|------|
| [SimplestAxis.cpp](../example/Gizmo/SimplestAxis.cpp) | — | 最简坐标轴：从 (0,0,0) 沿三轴各画一条直线，用于确认坐标轴方向 |
| [PlaneGrid3D.cpp](../example/Gizmo/PlaneGrid3D.cpp) | — | 一个网格 Mesh 使用**三个不同 MaterialInstance** 及不同 Transform 绘制 |
| [RayPicking.cpp](../example/Gizmo/RayPicking.cpp) | — | 全静态对象场景，但**动态修改 VAB/VBO**（演示运行时顶点数据更新）|
| [GizmoUsageExample.cpp](../example/Gizmo/GizmoUsageExample.cpp) | — | Gizmo 组件完整测试：同材质不同 MI 一个 DrawCall，+ **OverrideMaterial** 高亮显示 |

### 纹理示例（example/Texture/）

| 文件 | 窗口标题 | 说明 |
|------|----------|------|
| [texture_quad.cpp](../example/Texture/texture_quad.cpp) | Draw a quad with texture | 带纹理的四边形（ECS），最简 2D 纹理范例 |
| [texture_rect.cpp](../example/Texture/texture_rect.cpp) | — | 单个 2D 矩形 + 纹理绘制（ECS）|
| [texture_rect_array.cpp](../example/Texture/texture_rect_array.cpp) | Draw many rectangle with texture | **Texture2DArray**：4 张不同纹理绑定为数组，**1 DrawCall 画 4 个不同纹理矩形** |

### GUI / 文本示例（example/GUI/）

| 文件 | 窗口标题 | 说明 |
|------|----------|------|
| [DrawText_ECS.cpp](../example/GUI/DrawText_ECS.cpp) | DrawText_ECS | 2D 文本绘制（`TextComponent` + `TextRenderPipeline`，可完美运行，非最终版）|
| [DrawMultiLineText_ECS.cpp](../example/GUI/DrawMultiLineText_ECS.cpp) | — | 多段 2D 文本（可运行，**字符颜色效果有误**）|
| [DrawRoundrectangle.cpp](../example/GUI/DrawRoundrectangle.cpp) | — | 圆角矩形：通过控制尺寸和各角半径可绘制正圆/矩形/圆角矩形（UI 基础绘图元件）|

### 几何体示例（example/Geometry/）

| 文件 | 窗口标题 | 说明 |
|------|----------|------|
| [ExtrudedPolygonTest.cpp](../example/Geometry/ExtrudedPolygonTest.cpp) | Extruded Polygon | **2D 多边形挤压为 3D**：三角形与五边形挤压演示（`ExtrudedPolygonCreateInfo`）|
| [GeometryTest.cpp](../example/Geometry/GeometryTest.cpp) | Render Geometry | 加载转换好的 `.geometry` 文件（Chess 棋子模型），展示文件格式加载流程 |
| [LineRenderTest.cpp](../example/Geometry/LineRenderTest.cpp) | Wire Shape Test | **线条绘制**测试（专用 `LineRenderSystem` 系列）|
| [WallsFromPolyline.cpp](../example/Geometry/WallsFromPolyline.cpp) | Walls From Polyline Example | 从折线生成墙体：多段复杂折线（Z字形/矩形环/U形）程序化生成墙面几何 |
| [LoadGeometry.cpp](../example/Geometry/LoadGeometry.cpp) | — | 加载 `.geometry` 格式文件的底层工具函数实现 |
| [LoadScene.cpp](../example/Geometry/LoadScene.cpp) | — | 加载场景文件的底层工具函数实现 |

### 环境/天空示例（example/Environment/）

| 文件 | 窗口标题 | 说明 |
|------|----------|------|
| [04_SubWorldBuiltinGeometryECS.cpp](../example/Environment/04_SubWorldBuiltinGeometryECS.cpp) | SubWorld Builtin Geometry (ECS) | **SubWorld 嵌套**演示：子世界内置几何体（`SubWorldAnimatedGeometryModule`）|
| [AtmosphereSkyMinimal.cpp](../example/Environment/AtmosphereSkyMinimal.cpp) | SimplestAtmosphere | **最简大气天空**（Atmosphere Scattering 最小化实现）|
| [AtmosphereSkySunGizmo.cpp](../example/Environment/AtmosphereSkySunGizmo.cpp) | AtmosphereSkySunGizmo | 大气天空 + **太阳 Gizmo** 可视化交互调整太阳方向 |
| [BasicLitSunDirectionECS.cpp](../example/Environment/BasicLitSunDirectionECS.cpp) | BasicLit Sun Direction ECS | BasicLit 材质 ECS 多 Mesh + **SunDirectionControlSystem** 实时调整太阳方向控制光照 |

### 光照工具（example/LightBasic/）

| 文件 | 说明 |
|------|------|
| [test_luminousflux.cpp](../example/LightBasic/test_luminousflux.cpp) | `LuminousFlux` 光通量单位转换类功能测试（非图形渲染，纯数学逻辑）|

---

## 合批渲染的完整技术栈

```
Entity
  ├─ TransformComponent  ──► TransformAssignmentBuffer (Instance Rate VAB: mat_id)
  │                           └─ SSBO/UBO: mat4x4 data[N]
  └─ PrimitiveComponent
       ├─ Geometry (VAB + IBO)            ──► GeometryDataBuffer
       ├─ MaterialInstance (per-object)   ──► MaterialInstanceAssignmentBuffer (Instance Rate VAB: mi_id)
       │                                       └─ SSBO/UBO: MI data[N]
       └─ Primitive → VertexDataManager  ──► 大 VBO/IBO 块分配

Shader: layout(location=N) in uint transform_id;
        mat4 transform = transform_ssbo.data[transform_id];
        vec4 mi_color   = mi_ssbo.data[mi_id].color;

    + Texture2DArray (texture_id in MaterialInstance)
    + vkCmdDrawIndexedIndirect (Indirect Buffer)
    = 单 DrawCall 绘制同类所有 Mesh ✓
```

详见：
- [ecs/render-chain-analysis.md](ecs/render-chain-analysis.md)
- [ecs/render-pipeline-group.md](ecs/render-pipeline-group.md)