# Render To Texture Examples | 渲染到纹理示例

This directory contains two new examples demonstrating the "Render To Texture" technique in the ULRE Vulkan engine.

这个目录包含两个新的示例，演示在ULRE Vulkan引擎中的"渲染到纹理"技术。

## Examples | 示例

### 1. RenderToTexture.cpp
A comprehensive example with detailed documentation and error handling.

一个包含详细文档和错误处理的综合示例。

**Features | 功能:**
- Detailed bilingual comments (Chinese/English) | 详细的双语注释（中英文）
- Comprehensive error handling and logging | 综合错误处理和日志记录
- Structured code organization | 结构化的代码组织
- Step-by-step explanation of the render-to-texture process | 渲染到纹理过程的步骤说明

### 2. RenderToTextureSimple.cpp
A simplified version that closely follows the existing OffscreenRender.cpp pattern.

一个简化版本，紧密遵循现有的OffscreenRender.cpp模式。

**Features | 功能:**
- Minimal code, easy to understand | 最少代码，容易理解
- Follows existing ULRE patterns | 遵循现有的ULRE模式
- Direct adaptation of OffscreenRender example | OffscreenRender示例的直接改编

## How It Works | 工作原理

Both examples demonstrate the following process:
两个示例都演示了以下过程：

1. **Create Offscreen Render Target | 创建离屏渲染目标**
   - Create a 512x512 texture render target
   - 创建一个512x512纹理渲染目标

2. **Render to Texture | 渲染到纹理**
   - Render a colored triangle/shape to the offscreen texture
   - 将彩色三角形/形状渲染到离屏纹理

3. **Use Texture | 使用纹理**
   - Apply the rendered texture to a 3D cube
   - 将渲染的纹理应用到3D立方体

4. **Display Result | 显示结果**
   - Show the cube with the dynamically rendered texture
   - 显示带有动态渲染纹理的立方体

## Building | 构建

To enable these examples, uncomment the corresponding lines in `CMakeLists.txt`:

要启用这些示例，请在`CMakeLists.txt`中取消注释相应的行：

```cmake
CreateProject("Advanced Rendering" RenderToTexture       RenderToTexture.cpp)
CreateProject("Advanced Rendering" RenderToTextureSimple RenderToTextureSimple.cpp)
```

## Materials Used | 使用的材质

- **Offscreen rendering**: `res/material/VertexColor2D` | **离屏渲染**: `res/material/VertexColor2D`
- **Main scene**: `res/material/TextureMask3D` | **主场景**: `res/material/TextureMask3D`

## Key Concepts | 关键概念

- **Framebuffer Objects (FBO)** | **帧缓冲对象**
- **Render Targets** | **渲染目标**
- **Texture Sampling** | **纹理采样**
- **Multi-pass Rendering** | **多通道渲染**

These examples are useful for learning:
这些示例对学习以下内容很有用：

- Shadow mapping | 阴影映射
- Post-processing effects | 后处理效果
- Reflection/refraction effects | 反射/折射效果
- UI rendering | UI渲染
- Minimap/preview rendering | 小地图/预览渲染