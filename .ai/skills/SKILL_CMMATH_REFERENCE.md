# SKILL: CMMath 数学库完整 API 参考

> **何时使用本文档**：当你需要向量运算、矩阵构建、投影/视图矩阵、四元数、变换（Transform）、几何体（AABB/OBB/Frustum等）时。
> CMMath 是 ULRE 引擎的数学子模块（`CMMath/`），基于 GLM 封装，所有类型都在 `hgl::math` 命名空间。
>
> **坐标系约定（全局）**：Z 轴向上、右手系、Vulkan NDC（深度 [0,1]、Y 向下）。
> 相机在视图空间中朝向 **-Z 方向**（等价于 `glm::lookAtRH`）。

---

## ⚠️ 已知 Bug（使用前必读）

### Bug 1 🔴 透视投影矩阵 X 轴取反（严重）

**文件**：`CMMath/src/Math/Matrix4f.cpp`
**影响**：`PerspectiveMatrix`、`PerspectiveMatrixReversedZ`、`MakeInfiniteReversedZProj`

**问题**：这三个函数的 `m[0][0]` 均为 `-f/aspect`（负值），导致 NDC_x 在透视除法后与 view_x 符号相反，即渲染图像左右镜像。

```cpp
// ❌ 当前实现（有 Bug）
m[0][0] = -f / aspect_ratio;   // X 反转！屏幕右侧 → NDC 负值 → 左侧

// ✅ 应为（Vulkan RH，Y 翻转修正）
m[0][0] = +f / aspect_ratio;   // X 正确，屏幕右侧 → NDC 正值 → 右侧
m[1][1] = -f;                  // Y 取反是正确的：Vulkan NDC Y 向下
```

**代码注释误导**："矩阵形式与 glm::perspectiveRH_ZO 等价" 是错误的注释。`glm::perspectiveRH_ZO` 的 X 分量为正值。

**影响范围**：使用 `PerspectiveMatrix` 的所有渲染均会水平镜像；`GetFrustumPlanes` 提取的左右平面也会互换，导致裁剪错误。

---

### Bug 2 🔴 GetRotateMatrix / GetRotateQuat 声明与实现签名不匹配

**声明（Matrix.h / Quaternion.h）**：
```cpp
const Matrix4f GetRotateMatrix(const Vector3f &world_position,
                               const Vector3f &old_direction,
                               const Vector3f &new_direction);  // 3 个参数
const Quatf    GetRotateQuat  (const Vector3f &world_position,
                               const Vector3f &old_direction,
                               const Vector3f &new_direction);  // 3 个参数
```

**实现（Matrix4f.cpp）**：
```cpp
Matrix4f GetRotateMatrix(const Vector3f &old_direction,
                         const Vector3f &new_direction);  // 只有 2 个参数！
Quatf    GetRotateQuat  (const Vector3f &old_direction,
                         const Vector3f &new_direction);  // 只有 2 个参数！
```

**影响**：`CameraInfo.h::CalculateFacingRotationMatrix` 和 `CalculateFacingRotationQuat` 调用 3 参数版本，会产生链接错误（linker error）。

---

### Bug 3 🟡 IsometricViewMatrix 使用 Y-up（与引擎约定不符）

```cpp
// ❌ 当前（Y-up，与 Z-up 引擎约定不符）
Vector3f up(0, 1, 0);

// ✅ 应为
Vector3f up(0, 0, 1);  // Z-up
```

---

### Bug 4 🟡 TranslateMatrix(x, y) 使用 z=1.0f

```cpp
// ❌ 当前：z=1.0f 会沿 Z 轴（世界"上"方向）移动 1 个单位
return glm::translate(Matrix4f(1.0f), Vector3f(x, y, 1.0f));

// ✅ 应为：纯 XY 平移，不改变 Z
return glm::translate(Matrix4f(1.0f), Vector3f(x, y, 0.0f));
```

---

## 目录

1. [头文件速查表](#1-头文件速查表)
2. [向量类型（VectorTypes.h）](#2-向量类型vectortypesh)
3. [向量运算（VectorOperations.h / Vector.h）](#3-向量运算vectoroperationsh--vectorh)
4. [矩阵类型与工具（Matrix.h）](#4-矩阵类型与工具matrixh)
5. [投影与视图矩阵（Projection.h）](#5-投影与视图矩阵projectionh)
6. [四元数（Quaternion.h）](#6-四元数quaternionh)
7. [变换类（transform/Transform.h）](#7-变换类transformtransformh)
8. [几何体（geometry/）](#8-几何体geometry)
9. [动画数学（animation/）](#9-动画数学animation)
10. [其他工具](#10-其他工具)
11. [使用示例](#11-使用示例)
12. [坐标系与矩阵约定总结](#12-坐标系与矩阵约定总结)

---

## 1. 头文件速查表

| 功能 | 头文件 | 命名空间 |
|------|--------|---------|
| 向量类型 | `<hgl/math/VectorTypes.h>` | `hgl::math` |
| 向量运算 | `<hgl/math/VectorOperations.h>` | `hgl::math` |
| 向量工具 | `<hgl/math/VectorUtils.h>` | `hgl::math` |
| 矩阵操作 | `<hgl/math/Matrix.h>` | `hgl::math` |
| 投影/视图矩阵 | `<hgl/math/Projection.h>` | `hgl::math` |
| 四元数 | `<hgl/math/Quaternion.h>` | `hgl::math` |
| 变换类 | `<hgl/math/transform/Transform.h>` | `hgl::math` |
| AABB | `<hgl/math/geometry/AABB.h>` | `hgl::math` |
| OBB | `<hgl/math/geometry/OBB.h>` | `hgl::math` |
| 视锥体 | `<hgl/math/geometry/Frustum.h>` | `hgl::math` |
| 平面 | `<hgl/math/geometry/Plane.h>` | `hgl::math` |
| 射线 | `<hgl/math/geometry/Ray.h>` | `hgl::math` |
| 包围球 | `<hgl/math/geometry/BoundingSphere.h>` | `hgl::math` |
| 相机信息 | `<hgl/graph/CameraInfo.h>` | `hgl::graph` |
| 半浮点 | `<hgl/math/HalfFloat.h>` | `hgl` |
| 角度转换 | `<hgl/math/Angle.h>` | `hgl::math` |
| 三角函数常量 | `<hgl/math/TrigConstants.h>` | `hgl::math` |
| 插值 | `<hgl/math/Lerp1D.h>` 等 | `hgl::math` |
| 双四元数 | `<hgl/math/animation/DualQuaternion.h>` | `hgl::math` |
| 骨骼数学 | `<hgl/math/animation/SkeletonMath.h>` | `hgl::math` |

---

## 2. 向量类型（VectorTypes.h）

```cpp
#include <hgl/math/VectorTypes.h>
```

### 浮点向量

| 类型 | GLM 等价 | 说明 |
|------|----------|------|
| `Vector2f` | `glm::vec2` | 2D 浮点向量 |
| `Vector3f` | `glm::vec3` | 3D 浮点向量 |
| `Vector4f` | `glm::vec4` | 4D 浮点向量 |
| `Vector2d` | `glm::dvec2` | 2D 双精度向量 |
| `Vector3d` | `glm::dvec3` | 3D 双精度向量 |

### 整型向量

| 类型 | 说明 |
|------|------|
| `Vector2i`, `Vector3i`, `Vector4i` | 默认 int |
| `Vector2u`, `Vector3u`, `Vector4u` | 默认 uint |
| `Vector2i8` ~ `Vector4i64` | 指定位宽整型 |
| `Vector2u8` ~ `Vector4u64` | 指定位宽无符号 |

### 预定义常量

```cpp
// 零向量
ZeroVector2f   // (0,0)
ZeroVector3f   // (0,0,0)
ZeroVector4f   // (0,0,0,0)

// 单位向量
OneVector3f    // (1,1,1)

// 轴向量（Z-up 坐标系）
AxisVector::X  // (1,0,0) — 东/右
AxisVector::Y  // (0,1,0) — 北/前（俯视）
AxisVector::Z  // (0,0,1) — 上（引擎 up 方向）
```

---

## 3. 向量运算（VectorOperations.h / Vector.h）

```cpp
#include <hgl/math/VectorOperations.h>
#include <hgl/math/Vector.h>
```

### 基础运算

```cpp
// 归一化
Vector3f n = Normalized(v);           // 返回归一化向量
void     Normalize(v);                // 原地归一化

// 长度
float len   = Length(v);              // 向量长度
float lenSq = LengthSquare(v);        // 长度平方（更快，避免 sqrt）

// 点积 / 叉积
float d = Dot(a, b);
Vector3f c = Cross(a, b);             // 右手叉积

// 距离
float dist = Distance(a, b);
float distSq = DistanceSquare(a, b);
```

### 精度比较

```cpp
bool IsNearlyEqual(const Vector3f &a, const Vector3f &b, float err = float_error);
bool IsNearlyZero(const Vector3f &v, float err = float_error);
```

### 向量变换

```cpp
// 用矩阵变换位置（含平移）
Vector3f tp = TransformPosition(mat4, v);

// 用矩阵变换方向（不含平移）
Vector3f td = TransformDirection(mat4, v);

// 用矩阵变换法线（逆转置）
Vector3f tn = TransformNormal(mat4, v);
Vector3f tn = TransformNormal(mat3, v);
```

---

## 4. 矩阵类型与工具（Matrix.h）

```cpp
#include <hgl/math/Matrix.h>
```

### 矩阵类型

| 类型 | GLM 等价 | 字节常量 |
|------|----------|---------|
| `Matrix2f` | `glm::mat2` | `Matrix2fBytes` |
| `Matrix3f` | `glm::mat3` | `Matrix3fBytes` |
| `Matrix4f` | `glm::mat4` | `Matrix4fBytes` |
| `Matrix3x4f` | `glm::mat3x4` | — |
| `Matrix4x3f` | `glm::mat4x3` | — |

```cpp
// 单位矩阵常量
Identity2f
Identity3f
Identity4f

// 判断是否为单位矩阵
bool IsIdentityMatrix(const Matrix4f &m);
bool IsNearlyEqual(const Matrix4f &m1, const Matrix4f &m2, float err = float_error);
```

### 矩阵构建

```cpp
// ── 平移矩阵 ──────────────────────────────────────────
Matrix4f TranslateMatrix(const Vector3f &v);
Matrix4f TranslateMatrix(float x, float y, float z);
// ⚠️ TranslateMatrix(x, y) 有 Bug，z=1.0f，勿用于 2D

// ── 缩放矩阵 ──────────────────────────────────────────
Matrix4f ScaleMatrix(const Vector3f &v);
Matrix4f ScaleMatrix(float x, float y, float z);
Matrix4f ScaleMatrix(float s);                     // 均匀缩放
// ScaleMatrix(x, y) 使用 z=1.0f，2D 缩放正确（Z 不缩放）

// ── 旋转矩阵（4x4）────────────────────────────────────
Matrix4f AxisXRotate(float rad);                   // 绕 X 轴
Matrix4f AxisYRotate(float rad);                   // 绕 Y 轴
Matrix4f AxisZRotate(float rad);                   // 绕 Z 轴
Matrix4f AxisRotate(float rad, const Vector3f &axis); // 任意轴
Matrix4f AxisRotate(float rad, float x, float y, float z);

// ── 旋转矩阵（3x3）────────────────────────────────────
Matrix3f AxisRotate3f(float rad, const Vector3f &axis);
Matrix3f AxisRotate3fDeg(float deg, const Vector3f &axis);   // 度数版本

// ── 四元数转矩阵 ─────────────────────────────────────
Matrix4f ToMatrix(const Quatf &quat);

// ── 组合变换矩阵 ──────────────────────────────────────
// 推荐：TRS 顺序（平移 × 旋转 × 缩放）
Matrix4f MakeMatrix(const Vector3f &move,
                    const Quatf &rotate_quat,
                    const Vector3f &scale_xyz);

Matrix4f MakeMatrix(const Vector3f &move,
                    const Vector3f &rotate_axis,
                    float rotate_angle_degrees,
                    const Vector3f &scale_xyz);
```

### 矩阵运算

```cpp
// 求逆 / 转置
Matrix4f inv = Inverse(m);
Matrix4f t   = Transpose(m);

// 相对矩阵（从 reference 坐标系到 self 坐标系）
Matrix4f rel = RelativeMatrix(self_matrix, reference_matrix);
// 等价于：inverse(reference) * self

// 旋转矩阵（将 old_direction 旋转到 new_direction）
// ⚠️ 注意：下面两个函数的头文件声明有 3 个参数（world_position），
//    但实现只有 2 个参数。直接用 2 参数版本：
Matrix4f rot = GetRotateMatrix(old_dir, new_dir);   // 实现版本
Quatf    q   = GetRotateQuat  (old_dir, new_dir);   // 实现版本

// 变换分解
Vector3f t; Quatf r; Vector3f s;
bool ok = DecomposeTransform(mat, t, r, s);
```

---

## 5. 投影与视图矩阵（Projection.h）

```cpp
#include <hgl/math/Projection.h>
```

### 坐标系约定

| 空间 | X | Y | Z | 说明 |
|------|---|---|---|------|
| 世界空间 | 右 | 前 | **上** | Z-up 右手 |
| 视图空间 | 右 | 上 | **相机前方（-Z）** | 相机朝 -Z 看 |
| Vulkan NDC | 右→+1 | 下→+1 | 深度 [0,1] | Y 向下，深度从 0 到 1 |

### LookAt 视图矩阵

```cpp
// 产生右手视图矩阵（等价于 glm::lookAtRH）
// 默认 up = AxisVector::Z（Z 轴向上）
Matrix4f LookAtMatrix(const Vector3f &eye,
                      const Vector3f &target,
                      const Vector3f &up = AxisVector::Z);
```

**内部实现**：
```
forward = normalize(target - eye)
right   = normalize(cross(forward, up))   // cross(forward, Z)
nup     = cross(right, forward)
```

**⚠️ 注意**：当相机直视正上方或正下方（`forward ∥ up`）时会产生退化，避免 eye 在 target 正上/下方。

### 正交投影矩阵

```cpp
// 6 参数版本（用于 3D 场景正交投影）
// bottom/top 对应 view space Y 轴
// ⚠️ 对于对称视锥（如 -h 到 h），m[1][1] = 2/(bottom-top) 为负值，这是正确的（Vulkan Y 向下）
Matrix4f OrthoMatrix(float left, float right,
                     float bottom, float top,
                     float znear, float zfar);

// 4 参数版本：等价于 OrthoMatrix(0, width, height, 0, znear, zfar)
// 适用于 UI / 屏幕坐标（像素坐标，Y 向下）
Matrix4f OrthoMatrix(float width, float height, float znear, float zfar);

// 2 参数版本：znear=0, zfar=1（UI 专用）
Matrix4f OrthoMatrix(float width, float height);

// Reversed-Z 版本（近平面→1, 远平面→0）
Matrix4f OrthoMatrixReversedZ(float left, float right, float bottom, float top,
                               float znear, float zfar);
Matrix4f OrthoMatrixReversedZ(float width, float height, float znear, float zfar);
Matrix4f OrthoMatrixReversedZ(float width, float height);
```

**正交投影 m[1][1] 符号说明**：
- UI 用 `OrthoMatrix(w, h)`：bottom=h, top=0 → `m[1][1] = 2/(h-0) > 0`，像素 y=0（顶）→ NDC -1 ✓
- 3D 对称 `OrthoMatrix(-10,10,-10,10,...)`: bottom=-10, top=10 → `m[1][1] = 2/(-10-10) < 0`，上方 → NDC -1 ✓

### 透视投影矩阵

```cpp
// ⚠️ 注意 Bug 1：m[0][0] = -f/aspect（X 取反），待修复
// 当前行为：渲染结果水平镜像
// 参数：FOV 为垂直视角（度），znear/zfar 为正值
Matrix4f PerspectiveMatrix(float field_of_view,   // 垂直 FOV（度）
                           float aspect_ratio,     // 宽高比
                           float znear,
                           float zfar);

// Reversed-Z 版本（近→1, 远→0）
// ⚠️ 同样有 X 取反 Bug
Matrix4f PerspectiveMatrixReversedZ(float field_of_view,
                                    float aspect_ratio,
                                    float znear,
                                    float zfar);
```

**透视矩阵关键值**（修复前/后对比）：

| 元素 | 当前值（有 Bug） | 正确值 |
|------|----------------|--------|
| m[0][0] | `-f/aspect` | `+f/aspect` |
| m[1][1] | `-f` | `-f`（正确，Vulkan Y 翻转）|
| m[2][2] | `far/(near-far)` | `far/(near-far)` ✓ |
| m[2][3] | `-1` | `-1` ✓（w = -z_view > 0）|
| m[3][2] | `near*far/(near-far)` | `near*far/(near-far)` ✓ |

### 无限远 Reversed-Z 投影

```cpp
// 在 Camera.cpp 中使用，Reversed-Z + 无限远平面
// 近平面→1，∞→0，配合 VK_COMPARE_OP_GREATER 使用
// ⚠️ 同样有 X 取反 Bug（m[0][0] = -f/aspect）
Matrix4f MakeInfiniteReversedZProj(float fov_y_radians,  // 注意：弧度！
                                   float aspect,
                                   float near_z);
```

### 特殊投影矩阵

```cpp
// 等距视角（Isometric）投影矩阵
// use_precise_ratio=true 使用 arctan(1/√2)≈35.264°（精确 2:1 比例）
// use_precise_ratio=false 使用传统 30°
Matrix4f IsometricMatrix(float width, float height,
                         float znear, float zfar,
                         bool use_precise_ratio = true);
Matrix4f IsometricMatrix(float width, float height);  // 默认深度 0.1~100

// ⚠️ IsometricViewMatrix 有 Bug：使用 Y-up，应改为 Z-up
Matrix4f IsometricViewMatrix(const Vector3f& center, float distance);

// 正交→透视矩阵转换（用于等距→透视过渡）
Matrix4f IsometricToPerspectiveMatrix(float iso_width, float iso_height,
                                      float znear, float zfar,
                                      float fov_override = 0.0f);

// 自定义透视（z==0 时与正交一致，z 偏移时产生透视）
Matrix4f PerspectiveMatchOrtho(float left, float right, float bottom, float top,
                               float znear, float zfar, float alpha);
```

### 坐标投影/反投影

```cpp
// 世界坐标 → 屏幕像素坐标
Vector2i ProjectToScreen(const Vector3f& world_pos,
                         const Matrix4f& view,
                         const Matrix4f& projection,
                         const Vector2u& viewport_size);

// 屏幕坐标 → 世界坐标（在近平面）
Vector3f UnProjectToWorld(const Vector2i& win_pos,
                          const Matrix4f& view,
                          const Matrix4f& projection,
                          const Vector2u& viewport_size,
                          bool reversed_z = false);
```

---

## 6. 四元数（Quaternion.h）

```cpp
#include <hgl/math/Quaternion.h>
```

### 类型定义

```cpp
using Quatf = glm::quat;   // w, x, y, z
const Quatf IdentityQuatf(1, 0, 0, 0);  // 单位四元数
```

### 创建与分解

```cpp
// 从轴角创建（angle 为度数）
Quatf q = QuatFromAxisAngle(float angle_degrees, const Vector3f& axis);

// 分解轴角（angle 输出为度数）
void ExtractQuat(const Quatf& q, Vector3f& axis, float& angle_degrees);
const Vector3f& GetRotateAxis(const Quatf& q);
float           GetRotateAngle(const Quatf& q);   // 返回度数

// 四元数 → 矩阵（在 Matrix.h 中）
Matrix4f ToMatrix(const Quatf& q);

// DirectionToRotation：从方向向量创建旋转四元数
Quatf DirectionToRotation(const Vector3f& dir);
Vector3f RotationToDirection(const Quatf& rot);
```

### 插值

```cpp
// 线性插值
Quatf LerpQuat(const Quatf& from, const Quatf& to, float t);

// 球面线性插值（推荐用于旋转动画）
Quatf SLerpQuat(const Quatf& from, const Quatf& to, float t);
```

### 旋转矩阵/四元数（正确用法）

```cpp
// ⚠️ 头文件声明了 3 参数版本（含 world_position）但实现只有 2 参数
// 应直接使用 2 参数版本：
Matrix4f GetRotateMatrix(const Vector3f& old_direction, const Vector3f& new_direction);
Quatf    GetRotateQuat  (const Vector3f& old_direction, const Vector3f& new_direction);
```

---

## 7. 变换类（transform/Transform.h）

```cpp
#include <hgl/math/transform/Transform.h>
```

### Transform 类

`Transform` 懒计算 TRS（Translation × Rotation × Scale）矩阵，带版本号追踪脏状态。

```cpp
// ── 构建 ──────────────────────────────────────────────
Transform t;                    // 单位变换
Transform t(const Matrix4f& m); // 从矩阵分解

// ── 设置 TRS ──────────────────────────────────────────
t.SetTranslation(x, y, z);
t.SetTranslation(const Vector3f& v);

t.SetRotation(const Quatf& q);
t.SetRotation(const Vector3f& axis, float angle_radians);
t.SetRotation(AXIS axis, float angle_radians);    // AXIS::X/Y/Z
t.SetRotateAngle(float angle_radians);            // 仅更新角度
t.ClearRotation();

t.SetScale(float s);             // 均匀缩放
t.SetScale(float x, float y, float z);
t.SetScale(const Vector3f& v);

// ── 获取 ──────────────────────────────────────────────
const Vector3f& tr = t.GetTranslation();
const Vector3f& sc = t.GetScale();
const Quatf&    rq = t.GetRotationQuat();
const Vector3f& ra = t.GetRotationAxis();
float           ang = t.GetRotateAngle();    // 返回弧度

// ── 矩阵获取（自动更新）──────────────────────────────
const Matrix4f& m   = t.GetMatrix();         // 触发懒计算
const Matrix4f& inv = t.GetInverseMatrix();
uint32 ver = t.GetVersion();                 // 版本号追踪

// ── 变换操作 ─────────────────────────────────────────
Vector3f pos = t.TransformPosition  (v);   // 含平移
Vector3f dir = t.TransformDirection (v);   // 不含平移
Vector3f nrm = t.TransformNormal    (v);   // 逆转置法线

Vector3f pos = t.InverseTransformPosition(v);
Vector3f dir = t.InverseTransformDirection(v);
Vector3f nrm = t.InverseTransformNormal  (v);

Matrix4f child_m = t.TransActionMatrix(child_mat4);   // parent * child
Transform child_t = t.TransformTransform(child_t);    // 复合变换

// ── 插值 ─────────────────────────────────────────────
Transform result = Lerp(from, to, t);   // TRS 各分量分别插值

const Transform IdentityTransform;
```

---

## 8. 几何体（geometry/）

### AABB（轴对齐包围盒）

```cpp
#include <hgl/math/geometry/AABB.h>

struct AABB {
    Vector3f minPoint;
    Vector3f maxPoint;
    // 方法：
    void    Clear();
    void    Expand(const Vector3f& p);
    void    Expand(const AABB& other);
    bool    Contains(const Vector3f& p) const;
    bool    Intersects(const AABB& other) const;
    Vector3f GetCenter() const;
    Vector3f GetExtent() const;   // half-size
    float   GetVolume() const;
    AABB    Transform(const Matrix4f& m) const;
};
```

### OBB（有向包围盒）

```cpp
#include <hgl/math/geometry/OBB.h>
// OBB 由中心、三个轴、半尺寸定义
// SetFromPoints()、IntersectsOBB()、Contains() 等方法可用
```

### Frustum（视锥体）

```cpp
#include <hgl/math/geometry/Frustum.h>

// 从 VP 矩阵提取 6 个裁剪平面（Gribb & Hartmann 算法）
// 约定：z ∈ [0, w]（Vulkan Depth）
void GetFrustumPlanes(FrustumPlanes& planes, const Matrix4f& vp);

// Frustum 类（包含 SetMatrix、Contains、Intersects）
class Frustum {
    enum class Side { Left, Right, Top, Bottom, Front, Back };
    void SetMatrix(const Matrix4f& vp);
    bool Contains(const Vector3f& point) const;
    bool Intersects(const AABB& aabb) const;
    // ⚠️ Top/Bottom 命名基于 OpenGL Y-up 约定，Vulkan 中"Top"对应 NDC y>0（屏幕下方）
};
```

### BoundingSphere（包围球）

```cpp
#include <hgl/math/geometry/BoundingSphere.h>
struct BoundingSphere { Vector3f center; float radius; };
```

### Plane（平面）

```cpp
#include <hgl/math/geometry/Plane.h>
struct Plane { Vector4f equation; };   // (normal.x, normal.y, normal.z, d)
// ax + by + cz + d = 0
```

### Ray（射线）

```cpp
#include <hgl/math/geometry/Ray.h>
struct Ray { Vector3f origin; Vector3f direction; };
// Intersect 函数可查交 AABB/OBB/Plane/Triangle 等
```

### 查询工具

```cpp
#include <hgl/math/geometry/queries/CollisionDetector.h>
#include <hgl/math/geometry/queries/ContainmentQuery.h>
#include <hgl/math/geometry/queries/DistanceQuery.h>
#include <hgl/math/geometry/queries/RaycastQuery.h>
```

### 空间结构

```cpp
#include <hgl/math/spatial/BVH.h>      // 层次包围盒
#include <hgl/math/spatial/Octree.h>   // 八叉树
#include <hgl/math/spatial/QuadTree.h> // 四叉树
```

---

## 9. 动画数学（animation/）

```cpp
#include <hgl/math/animation/DualQuaternion.h>
#include <hgl/math/animation/SkeletonMath.h>

// DualQuaternion：刚体变换的双四元数表示
// SkeletonMath：骨骼蒙皮权重、混合矩阵等

// 双四元数：r=旋转四元数，t=平移四元数（0, tx/2, ty/2, tz/2）
struct DualQuaternion { Quatf real, dual; };
```

---

## 10. 其他工具

### 角度转换

```cpp
#include <hgl/math/Angle.h>
// deg2rad(degrees) → radians
// rad2deg(radians) → degrees
```

### 三角函数常量

```cpp
#include <hgl/math/TrigConstants.h>
// deg2rad(), rad2deg() 函数在此定义
// HGL_PI, HGL_TWO_PI 等常量
```

### 插值函数

```cpp
#include <hgl/math/Lerp1D.h>   // 标量插值
#include <hgl/math/Lerp2D.h>   // 2D 向量插值
#include <hgl/math/Lerp3D.h>   // 3D 向量插值
#include <hgl/math/VectorLerp.h>

float r = Lerp(a, b, t);           // 线性插值
Vector3f r = Lerp(va, vb, t);
```

### 半浮点

```cpp
#include <hgl/math/HalfFloat.h>
uint16 h = FloatToHalf(float f);
float  f = HalfToFloat(uint16 h);
```

### 噪声

```cpp
#include <hgl/math/Noise.h>
// PerlinNoise, SimplexNoise
```

---

## 11. 使用示例

### 构建相机 VP 矩阵（Z-up 右手，Vulkan）

```cpp
#include <hgl/math/Matrix.h>
#include <hgl/math/Projection.h>

using namespace hgl::math;

// ── 视图矩阵 ─────────────────────────────────────────────────────────
Vector3f eye    = Vector3f(0, -10, 5);   // 相机位置（Y 后移，Z 高度）
Vector3f target = Vector3f(0, 0, 0);     // 看向原点
// up = AxisVector::Z（默认）

Matrix4f view = LookAtMatrix(eye, target);  // ≡ glm::lookAtRH，up=Z

// ── 投影矩阵 ─────────────────────────────────────────────────────────
// ⚠️ PerspectiveMatrix 当前有 X 取反 Bug，待修复
// 修复前先用 glm::perspectiveRH_ZO 代替：
float fov    = 60.0f;                    // 垂直 FOV（度）
float aspect = float(width) / height;
float znear  = 0.1f, zfar = 1000.0f;

Matrix4f proj = PerspectiveMatrix(fov, aspect, znear, zfar);
// 或使用 Reversed-Z（推荐，精度更好）：
Matrix4f proj = PerspectiveMatrixReversedZ(fov, aspect, znear, zfar);

// ── VP 矩阵（Camera.cpp 中的顺序）────────────────────────────────────
Matrix4f vp         = proj * view;          // ⚠️ 必须是 proj * view
Matrix4f inverse_vp = Inverse(vp);
```

### 构建 Transform 并提交 GPU

```cpp
#include <hgl/math/transform/Transform.h>
using namespace hgl::math;

Transform t;
t.SetTranslation(0, 0, 1);                           // 向上 1 单位（Z-up）
t.SetRotation(AxisVector::Z, glm::radians(45.0f));   // 绕 Z 轴旋转 45°
t.SetScale(2.0f);

const Matrix4f& m = t.GetMatrix();  // 懒计算，TRS 顺序
```

### AABB 可见性裁剪

```cpp
#include <hgl/math/geometry/Frustum.h>
#include <hgl/math/geometry/AABB.h>
using namespace hgl::math;

Frustum frustum;
frustum.SetMatrix(camera_info->vp);      // VP 矩阵提取 6 平面

AABB aabb;
aabb.minPoint = Vector3f(-1,-1,-1);
aabb.maxPoint = Vector3f( 1, 1, 1);

if (frustum.Intersects(aabb))
{
    // 可见，提交渲染
}
```

### 射线拾取

```cpp
#include <hgl/math/geometry/Ray.h>
#include <hgl/math/Projection.h>
using namespace hgl::math;

// 从屏幕坐标生成射线
Vector3f near_world = UnProjectToWorld(screen_pos, view, proj, viewport_size, false);
Vector3f far_world  = UnProjectToWorld(screen_pos, view, proj, viewport_size, true /* reversed_z=far */);
Ray ray { near_world, Normalized(far_world - near_world) };
```

---

## 12. 坐标系与矩阵约定总结

| 约定项 | 值 | 备注 |
|--------|-----|------|
| 世界坐标系 | Z-up 右手 | X=右, Y=前, Z=上 |
| 相机 up 向量默认值 | `AxisVector::Z` | `LookAtMatrix` 第 3 参数 |
| 视图空间方向 | 相机朝 -Z | `lookAtRH` 标准 |
| NDC 深度范围 | [0, 1] | Vulkan 标准（near→0） |
| NDC Y 方向 | Y=+1 在底部 | Vulkan Y 向下 |
| 投影矩阵 Y 翻转 | `m[1][1] = -f` | 所有透视矩阵均正确 |
| 投影矩阵 X ⚠️ | `m[0][0] = -f/aspect`（Bug） | 应为 `+f/aspect` |
| VP 矩阵组合顺序 | `projection * view` | Camera.cpp 第 29 行 |
| Reversed-Z 近平面 | NDC z = 1 | 配合 `VK_COMPARE_OP_GREATER` |
| GLM 矩阵存储 | 列主序 | `m[col][row]` |
| `Matrix4f(a,b,c,d,...)` | 按列输入 | 前 4 值 = 第 0 列 |

### 关键头文件引用路径

```cpp
// 最常用的一组
#include <hgl/math/Vector.h>           // 向量类型 + 运算
#include <hgl/math/Matrix.h>           // 矩阵类型 + 平移/旋转/缩放
#include <hgl/math/Projection.h>       // 投影矩阵 + LookAt
#include <hgl/math/transform/Transform.h>  // TRS 变换类
#include <hgl/graph/CameraInfo.h>      // CameraInfo 结构体（视图空间工具）
```
