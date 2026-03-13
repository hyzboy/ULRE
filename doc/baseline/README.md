# 回归测试基线

> **创建时机**：重构开始前（Step 1.1）
> **用途**：每个重构步骤完成后，对比截图/行为确认无视觉回退。

## 操作流程

1. 编译全部 example（Debug + Release）
2. 逐个运行，截图保存到本目录
3. 对照下表检查

## Example 清单

| # | 目录 | 可执行文件 | 预期行为 | 截图文件 |
|---|------|-----------|---------|---------|
| 1 | Basic | 01_draw_triangle | 显示彩色三角形 | basic_01.png |
| 2 | Basic | 02_auto_instance | 自动实例化渲染 | basic_02.png |
| 3 | Basic | 03_auto_merge_material_instance | 材质实例合并 | basic_03.png |
| 4 | Basic | 04_clock_use | 时钟显示 | basic_04.png |
| 5 | Basic | 05_Billboard | Billboard 渲染 | basic_05.png |
| 6 | Basic | 05b_BillboardECS | Billboard ECS 渲染 | basic_05b.png |
| 7 | Basic | 05c_BillboardPerspectiveECS | 透视 Billboard | basic_05c.png |
| 8 | Basic | 05d_FacingMeshBillboardECS | 面向摄像机 | basic_05d.png |
| 9 | Basic | 05e_FacingMeshBillboardZECS | Z 轴面向摄像机 | basic_05e.png |
| 10 | Basic | 06_SimpleCube | 简单立方体 | basic_06.png |
| 11 | Basic | 06b_BasicLitMeshesECS | 基础光照网格 | basic_06b.png |
| 12 | Basic | 06c_TextureBlinnPhongMeshesECS | BlinnPhong 纹理网格 | basic_06c.png |
| 13 | Basic | 06d_PBRSpheresECS | PBR 球体 | basic_06d.png |
| 14 | Basic | 07_RenderBoundBox | 包围盒渲染 | basic_07.png |
| 15 | Basic | 09_RecursiveCube | 递归立方体 | basic_09.png |
| 16 | Texture | 05_texture_format | 纹理格式列表 | tex_05.png |
| 17 | Texture | 06_texture_quad | 纹理四边形 | tex_06.png |
| 18 | Texture | 07_texture_rect | 纹理矩形 | tex_07.png |
| 19 | Texture | 08_texture_rect_array | 纹理矩形数组 | tex_08.png |
| 20 | Gizmo | 01_SimplestAxis | 坐标轴 Gizmo | gizmo_01.png |
| 21 | Gizmo | 02_PlaneGrid3D | 3D 平面网格 | gizmo_02.png |
| 22 | Gizmo | 03_RayPicking | 射线拾取 | gizmo_03.png |
| 23 | Gizmo | 05_GizmoUsageExample | Gizmo 使用示例 | gizmo_05.png |
| 24 | GUI | 04.TextDrawTest_ECS | ECS 文本绘制 | gui_04.png |
| 25 | GUI | 05.DrawMultiLineText_ECS | 多行文本 | gui_05.png |
| 26 | Geometry | 01_ExtrudedPolygonTest | 挤出多边形 | geo_01.png |
| 27 | Geometry | 03_WallsFromPolyline | 多段线墙体 | geo_03.png |
| 28 | Geometry | 04_LineRenderTest | 线渲染 | geo_04.png |
| 29 | Geometry | 05_LoadGeometry | 加载几何体 | geo_05.png |
| 30 | Geometry | 06_LoadScene | 加载场景 | geo_06.png |
| 31 | Environment | 01_AtmosphereSkyMinimal | 大气天空（最小） | env_01.png |
| 32 | Environment | 02_AtmosphereSkySunGizmo | 大气天空+太阳 | env_02.png |
| 33 | Environment | 03_BasicLitSunDirectionECS | 太阳方向光照 | env_03.png |
| 34 | Environment | 04_SubWorldBuiltinGeometryECS | SubWorld 内建几何 | env_04.png |
| 35 | LightBasic | 01_BlinnPhongDirectionLight | BlinnPhong 方向光 | light_01.png |

## 验证清单

```
□ 所有 example 编译通过 (Debug)
□ 所有 example 编译通过 (Release)
□ 逐个运行截图已保存
□ 无运行时崩溃或明显渲染错误
```

> **注意**：截图需要手动执行。本文件由重构自动化流程生成。
