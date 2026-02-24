# CMMath 文件分类 - 快速参考

## 当前文件地图

```
CMMath/inc/hgl/math/
├─ 向量核心
│  ├─ VectorTypes.h        ✅ 向量类型定义
│  ├─ VectorOperations.h   ✅ 比较、点积、叉积、长度
│  ├─ VectorUtils.h        ⚠️  维度转换、极值、插值(需分离)
│  ├─ VectorLerp.h         ⚠️  向量插值(与Lerp1D/2D/3D重复?)
│  ├─ VectorConversion.h   ⚠️  标量转换(名称误导，应叫ScalarConversion)
│  └─ Vector.h             ✅ 统一入口
│
├─ 数值工具
│  ├─ Clamp.h              ⚠️  标量/向量钳制(混杂)
│  ├─ FloatPrecision.h     ✅ 浮点精度常量
│  ├─ MathUtils.h          ⚠️  浮点截取+体积计算(混杂)
│  └─ [缺失]ScalarUtils.h  📋 应有此文件
│
├─ 插值系列
│  ├─ Lerp1D.h             ✅ 标量插值
│  ├─ Lerp2D.h             ✅ Vector2f插值
│  ├─ Lerp3D.h             ✅ Vector3f插值
│  └─ LerpType.h           ✅ 插值类型定义
│
├─ 颜色和混合
│  ├─ Color.h              ✅ 颜色类型、转换、常量
│  ├─ AlphaBlend.h         ✅ 混合函数库
│  └─ AlphaBlendMode.h     ✅ 混合模式枚举
│
├─ 变换和矩阵
│  ├─ Angle.h              ✅ 角度类型、转换
│  ├─ Matrix.h             ✅ 矩阵操作
│  └─ Quaternion.h         ✅ 四元数操作
│
└─ 其他
   ├─ Transform2D.h        ✅ 2D变换
   ├─ Random.h             ✅ 随机数
   ├─ Noise.h              ✅ 噪声生成
   ├─ SIMD.h               ✅ SIMD优化
   └─ geometry/            ✅ 几何体集合
```

## 关键问题清单

| # | 问题 | 影响 | 优先级 | 建议 |
|---|------|------|--------|------|
| 1 | ClampByte 和 ClampU8 重复 | 维护困难 | 🔴高 | 合并或别名 |
| 2 | VectorConversion.h 名称误导 | 理解困难 | 🔴高 | 改名为ScalarConversion |
| 3 | MathUtils 混杂浮点和体积 | 组织混乱 | 🔴高 | 拆分，体积移到geometry |
| 4 | Clamp.h 混杂标量和向量 | 理解困难 | 🟡中 | 分离或添加说明 |
| 5 | Lerp系列与VectorLerp关系不清 | 维护困难 | 🟡中 | 澄清分工或整合 |
| 6 | VectorUtils 包含插值 | 组织混乱 | 🟡中 | 移到VectorLerp |

## 快速修复行动

### 立即执行（高优先级）

**1. 解决ClampByte重复**
```cpp
// Clamp.h - 保留并确保导出
inline uint8_t ClampU8(float value) { ... }

// ScalarConversion.h - 使用别名或导入
using ClampByte = ClampU8;
```

**2. 重命名VectorConversion.h**
```
VectorConversion.h → ScalarConversion.h
更新所有导入语句
```

**3. 分离MathUtils**
```
MathUtils.h - 保留: hgl_clip_float(), 通用工具
geometry/GeometryCalculations.h - 新增: 体积计算
```

### 后续整理（中优先级）

**4. 规范VectorUtils**
- 移除: LerpDirection() → VectorLerp.h
- 添加: ClampVectorU8/U16/etc → 从Clamp.h

**5. 澄清Lerp系列**
- 确认: Lerp1D/2D/3D 与 VectorLerp 的确切分工
- 考虑: 新建 ColorLerp.h 或重新定位 VectorLerp.h

## 文件依赖链

```
VectorTypes.h
    ↓
VectorOperations.h ← VectorUtils.h ← VectorConversion.h
    ↓
VectorLerp.h
    ↓
Lerp1D/2D/3D.h ← Color.h ← AlphaBlend.h
                    ↓
                AlphaBlendMode.h

[独立] Clamp.h ← MathUtils.h(混杂问题)
```

## 建议的理想结构

```
├─ core/             (核心向量)
│  ├─ VectorTypes.h
│  ├─ VectorOperations.h
│  ├─ Clamp.h        (标量钳制)
│  ├─ ScalarConversion.h  (u8↔f转换)
│  └─ Vector.h       (统一入口)
│
├─ vector/          (向量工具)
│  ├─ VectorUtils.h  (维度转换、极值、钳制)
│  ├─ VectorLerp.h   (向量插值)
│  └─ [ColorLerp.h]  (颜色向量插值-可选)
│
├─ interpolation/   (插值库)
│  ├─ Lerp1D.h
│  ├─ Lerp2D.h
│  ├─ Lerp3D.h
│  └─ LerpType.h
│
├─ color/          (颜色和混合)
│  ├─ Color.h
│  ├─ AlphaBlend.h
│  └─ AlphaBlendMode.h
│
└─ ...             (其他不变)
```

