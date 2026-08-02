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
| 1 | Basic | DrawTriangle | 显示彩色三角形 | basic_01.png |
| 2 | Basic | AutoInstance | 自动实例化渲染 | basic_02.png |
| 3 | Basic | AutoMergeMaterialInstance | 材质实例合并 | basic_03.png |
| 4 | Basic | ClockUse | 时钟显示 | basic_04.png |
| 5 | Basic | Billboard | Billboard 渲染 | basic_05.png |
| 6 | Basic | Billboard | Billboard ECS 渲染 | basic_05b.png |
| 7 | Basic | BillboardPerspective | 透视 Billboard | basic_05c.png |
| 8 | Basic | FacingMeshBillboard | 面向摄像机 | basic_05d.png |
| 9 | Basic | FacingMeshBillboardZ | Z 轴面向摄像机 | basic_05e.png |
| 10 | Basic | SimpleCube | 简单立方体 | basic_06.png |
| 11 | Basic | BasicLitMeshes | 基础光照网格 | basic_06b.png |
| 12 | Basic | TextureBlinnPhongMeshes | BlinnPhong 纹理网格 | basic_06c.png |
| 13 | Basic | PBRSpheres | PBR 球体 | basic_06d.png |
| 14 | Basic | RenderBoundBox | 包围盒渲染 | basic_07.png |
| 15 | Basic | RecursiveCube | 递归立方体 | basic_09.png |
| 16 | Texture | TextureFormat | 纹理格式列表 | tex_05.png |
| 17 | Texture | TextureQuad | 纹理四边形 | tex_06.png |
| 18 | Texture | TextureRect | 纹理矩形 | tex_07.png |
| 19 | Texture | TextureRectArray | 纹理矩形数组 | tex_08.png |
| 20 | Gizmo | SimplestAxis | 坐标轴 Gizmo | gizmo_01.png |
| 21 | Gizmo | PlaneGrid3D | 3D 平面网格 | gizmo_02.png |
| 22 | Gizmo | RayPicking | 射线拾取 | gizmo_03.png |
| 23 | Gizmo | GizmoUsageExample | Gizmo 使用示例 | gizmo_05.png |
| 24 | GUI | TextDrawTest | ECS 文本绘制 | gui_04.png |
| 25 | GUI | DrawMultiLineText | 多行文本 | gui_05.png |
| 26 | Geometry | ExtrudedPolygonTest | 挤出多边形 | geo_01.png |
| 27 | Geometry | WallsFromPolyline | 多段线墙体 | geo_03.png |
| 28 | Geometry | LineRenderTest | 线渲染 | geo_04.png |
| 29 | Geometry | LoadGeometry | 加载几何体 | geo_05.png |
| 30 | Geometry | LoadScene | 加载场景 | geo_06.png |
| 31 | Environment | AtmosphereSkyMinimal | 大气天空（最小） | env_01.png |
| 32 | Environment | AtmosphereSkySunGizmo | 大气天空+太阳 | env_02.png |
| 33 | Environment | BasicLitSunDirection | 太阳方向光照 | env_03.png |
| 34 | Environment | SubWorldBuiltinGeometry | SubWorld 内建几何 | env_04.png |
| 35 | LightBasic | BlinnPhongDirectionLight | BlinnPhong 方向光 | light_01.png |

## 验证清单

```
□ 所有 example 编译通过 (Debug)
□ 所有 example 编译通过 (Release)
□ 逐个运行截图已保存
□ 无运行时崩溃或明显渲染错误
```

> **注意**：截图需要手动执行。本文件由重构自动化流程生成。
