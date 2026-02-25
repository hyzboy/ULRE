# 将 Blend.h 中的非 Blend 相关函数独立出来

## 概述
将 CM2D 模块中 Blend.h 文件里的非 Blend 相关的转换函数独立提取到 CMMath 模块中，以便在其他模块中也能使用这些通用的转换工具。

## 迁移的函数

### 字节/浮点转换
- `ClampByte()` - 将浮点值夹紧到 uint8 范围 [0, 255]
- `ByteToFloat()` - 将 uint8 [0,255] 转换为 float [0,1]
- `FloatToByte()` - 将 float [0,1] 转换为 uint8 [0,255]（带夹紧）

### Vector3 转换
- `Vector3u8ToFloat()` - 将 Vector3u8 [0-255] 转换为 Vector3f [0.0-1.0]
- `Vector3fToByte()` - 将 Vector3f [0.0-1.0] 转换为 Vector3u8 [0-255]

### Vector4 转换  
- `Vector4u8ToFloat()` - 将 Vector4u8 [0-255] 转换为 Vector4f [0.0-1.0]
- `Vector4fToByte()` - 将 Vector4f [0.0-1.0] 转换为 Vector4u8 [0-255]

## 文件改动

### 新建文件
- **路径**: `CMMath/inc/hgl/math/ColorConversion.h`
- **内容**: 包含上述所有颜色转换函数的声明和实现
- **命名空间**: `hgl::math`

### 修改文件
- **路径**: `CM2D/inc/hgl/2d/Blend.h`
- **改动**:
  1. 添加 `#include<hgl/math/ColorConversion.h>` 包含新的头文件
  2. 在 bitmap 命名空间中添加 `using` 声明来导入转换函数
  3. 移除了这些函数的原始定义和相关文档注释

## 好处

1. **代码复用**: 转换函数现在可以在 CMMath 和其他模块中使用
2. **关注点分离**: Blend.h 专注于 Blend 相关的功能
3. **模块化**: ColorConversion.h 是一个独立的、可重用的模块
4. **易于维护**: 颜色转换逻辑集中在一个地方

## 兼容性

- 所有现有的 Blend.h 用户无需改动代码
- 通过 `using` 声明，bitmap 命名空间内仍可直接使用这些函数名
- 其他模块可以包含 `<hgl/math/ColorConversion.h>` 来使用这些转换函数

## 验证

- ✅ 编译无错误
- ✅ 所有函数定义已移至 ColorConversion.h
- ✅ Blend.h 正确导入并导出转换函数
- ✅ 所有使用点已通过 `using` 声明正确绑定
