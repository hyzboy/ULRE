# 2D多边形挤压为3D多边形功能说明

## 概述

本功能实现了将任意2D封闭多边形挤压为3D多边形的算法，支持指定挤压轴向。主要包含以下功能：

1. **通用多边形挤压**: `CreateExtrudedPolygon` - 支持任意2D多边形
2. **矩形挤压**: `CreateExtrudedRectangle` - 矩形挤压成立方体
3. **圆形挤压**: `CreateExtrudedCircle` - 圆形挤压成圆柱体

## 坐标系说明

- **Z轴向上**: 默认坐标系以Z轴为上方向
- **顺时针为正面**: 从挤压轴正方向看，顶点按顺时针顺序排列为正面
- **局部坐标系**: 为不同挤压轴向自动构建合适的局部坐标系

### 轴向对应的局部坐标系

- **Z轴挤压** (0,0,1): X轴作为right，Y轴作为up
- **X轴挤压** (1,0,0): Y轴作为right，Z轴作为up  
- **Y轴挤压** (0,1,0): Z轴作为right，X轴作为up
- **任意轴向**: 自动计算正交坐标系

## 使用示例

### 1. 通用多边形挤压

```cpp
// 定义三角形顶点
Vector2f triangleVertices[3] = {
    {-0.8f, -0.5f},  // 左下
    { 0.8f, -0.5f},  // 右下
    { 0.0f,  0.8f}   // 顶部
};

// 配置挤压参数
ExtrudedPolygonCreateInfo epci;
epci.vertices = triangleVertices;
epci.vertexCount = 3;
epci.extrudeDistance = 1.2f;
epci.extrudeAxis = Vector3f(0, 0, 1);  // Z轴向上挤压
epci.generateCaps = true;              // 生成顶底面
epci.generateSides = true;             // 生成侧面
epci.clockwiseFront = true;            // 顺时针为正面

// 创建几何体
Primitive *prim = CreateExtrudedPolygon(pc, &epci);
```

### 2. 矩形挤压成立方体

```cpp
// 创建2×1.5×1的立方体，沿Z轴挤压
Primitive *cube = CreateExtrudedRectangle(pc, 2.0f, 1.5f, 1.0f, Vector3f(0, 0, 1));
```

### 3. 圆形挤压成圆柱体

```cpp
// 创建半径0.8，高度1.5的圆柱体，16段，沿Z轴挤压
Primitive *cylinder = CreateExtrudedCircle(pc, 0.8f, 1.5f, 16, Vector3f(0, 0, 1));
```

### 4. 不同轴向挤压

```cpp
// X轴方向挤压
Primitive *x_extrude = CreateExtrudedRectangle(pc, 1.0f, 1.0f, 2.0f, Vector3f(1, 0, 0));

// Y轴方向挤压  
Primitive *y_extrude = CreateExtrudedRectangle(pc, 1.0f, 1.0f, 2.0f, Vector3f(0, 1, 0));

// 任意方向挤压
Vector3f diagonal = Normalize(Vector3f(1, 1, 1));
Primitive *diag_extrude = CreateExtrudedRectangle(pc, 1.0f, 1.0f, 2.0f, diagonal);
```

## 技术实现要点

### 顶点生成
- 将2D多边形顶点转换到3D局部坐标系
- 生成底面和顶面顶点
- 侧面顶点按需复制（用于不同的法线计算）

### 法线计算
- **侧面法线**: 通过边向量与挤压轴的叉积计算
- **顶底面法线**: 分别为挤压轴的正负方向

### 索引生成
- **侧面**: 每个边生成2个三角形（4个顶点组成的四边形）
- **顶底面**: 使用三角扇形算法分解多边形

### 包围盒计算
- 遍历所有生成的3D顶点
- 计算最小和最大边界点

## 测试示例

`ExtrudedPolygonTest.cpp` 演示了各种用法：
- 矩形挤压成立方体
- 圆形挤压成圆柱体
- 三角形挤压
- 五边形沿X轴挤压

这些几何体被放置在不同位置，可以通过相机观察验证结果的正确性。