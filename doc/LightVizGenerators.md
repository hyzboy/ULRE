# 光源可视化线段生成器 (Light Visualization Line Generators)

这个模块为3D场景编辑器提供了各种光源和摄像机观察范围的线框可视化功能。所有生成器都使用简单的C语言风格，生成线段的起点和终点坐标，配合 `LineManager` 进行渲染。

## 功能特性

本模块包含以下几种线段生成器：

### 1. 点光源生成器 (`GeneratePointLightLines`)
- **功能**: 绘制点光源的影响范围
- **可视化**: 三个正交的圆圈（XY、XZ、YZ平面）+ 中心十字标记
- **参数**: 
  - `light`: `PointLight` 结构体（包含位置信息）
  - `radius`: 可视化范围半径
  - `color`: 线条颜色

### 2. 聚光灯生成器 (`GenerateSpotLightLines`)
- **功能**: 绘制聚光灯的圆锥形照射范围
- **可视化**: 圆锥底面圆圈 + 从光源到圆锥边缘的射线 + 光源位置标记
- **参数**:
  - `light`: `SpotLight` 结构体（包含位置、方向、照射角度）
  - `length`: 圆锥长度
  - `color`: 线条颜色

### 3. 矩形光源生成器 (`GenerateRectLightLines`)
- **功能**: 绘制面光源的矩形形状和朝向
- **可视化**: 矩形边框 + 对角线十字 + 方向指示箭头
- **参数**:
  - `position`: 光源位置
  - `direction`: 光源朝向方向（归一化）
  - `up`: 光源向上方向（归一化）
  - `width`, `height`: 矩形尺寸
  - `color`: 线条颜色

### 4. 摄像机视锥体生成器 (`GenerateCameraFrustumLines`)
- **功能**: 绘制摄像机的视野范围
- **可视化**: 近/远裁剪面矩形 + 连接边线 + 摄像机位置标记
- **参数**:
  - `position`: 摄像机位置
  - `direction`: 摄像机朝向方向（归一化）
  - `up`: 摄像机向上方向（归一化）
  - `fov_y`: 垂直视野角度（弧度）
  - `aspect`: 宽高比
  - `near_dist`, `far_dist`: 近/远裁剪面距离
  - `color`: 线条颜色

## 使用示例

```cpp
#include<hgl/graph/LightVizGenerators.h>
#include<hgl/graph/LineManager.h>

using namespace hgl::graph::light_viz_generators;

// 创建线条管理器
LineManager* line_mgr = new LineManager(render_framework);
line_mgr->Init();

// 1. 点光源可视化
PointLight point_light;
point_light.position = Vector3f(3, 2, 0);
GeneratePointLightLines(line_mgr, point_light, 1.5f, Color4f(1, 1, 0, 1));

// 2. 聚光灯可视化
SpotLight spot_light;
spot_light.position = Vector3f(-3, 3, 2);
spot_light.direction = Vector3f(0.5f, -0.7f, -0.5f);
spot_light.coscutoff = 0.8f;  // cos(36度)
GenerateSpotLightLines(line_mgr, spot_light, 4.0f, Color4f(0, 1, 1, 1));

// 3. 矩形光源可视化
Vector3f rect_pos(0, 4, -3);
Vector3f rect_dir = Normalize(Vector3f(0, -1, 0.2f));
Vector3f rect_up = Normalize(Vector3f(0, 0.2f, 1));
GenerateRectLightLines(line_mgr, rect_pos, rect_dir, rect_up, 
                      2.0f, 1.5f, Color4f(1, 0, 1, 1));

// 4. 摄像机视锥体可视化
Vector3f cam_pos(5, 5, 5);
Vector3f cam_dir = Normalize(Vector3f(-0.5f, -0.5f, -0.7f));
Vector3f cam_up(0, 1, 0);
GenerateCameraFrustumLines(line_mgr, cam_pos, cam_dir, cam_up,
                          PI * 0.3f, 16.0f/9.0f, 0.5f, 8.0f,
                          Color4f(1, 0.5f, 0, 1));

// 更新并渲染
line_mgr->Update();
```

## 辅助函数

模块还提供了几个辅助函数：

- `CrossProduct(a, b)`: 计算两个向量的叉积
- `Normalize(v)`: 归一化向量
- `GenerateCommonLightingSetup(light_mgr, center_pos, scale)`: 生成一个标准的三点照明设置
- `GenerateDebugAxisAtPosition(light_mgr, position, size, alpha)`: 在指定位置绘制调试用的坐标轴

## 便捷函数示例

```cpp
// 快速创建标准照明设置
GenerateCommonLightingSetup(line_mgr, Vector3f(0, 0, 0), 1.0f);

// 在关键位置添加调试标记
GenerateDebugAxisAtPosition(line_mgr, Vector3f(5, 0, 0), 0.5f, 0.7f);
```

## 文件结构

- `inc/hgl/graph/LightVizGenerators.h`: 头文件声明
- `src/SceneGraph/LightVizGenerators.cpp`: 实现文件
- `example/Basic/LightVizTest.cpp`: 完整使用示例

## 技术说明

1. **简单设计**: 所有函数都使用纯C风格实现，只负责生成线段起点和终点
2. **数学运算**: 使用基本的三角函数（sin, cos, tan）和向量运算
3. **兼容性**: 与现有的 `LineManager` 和 `Light` 结构完全兼容
4. **高效渲染**: 通过 `LineManager` 统一管理所有线段，避免重复的渲染设置

## 注意事项

- 所有方向向量参数需要预先归一化
- 线段颜色使用 `Color4f` 格式（RGBA，范围0-1）
- 调用生成器后需要调用 `LineManager::Update()` 来更新渲染数据
- 可以多次调用不同的生成器来组合显示多个光源