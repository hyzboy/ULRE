# Render To CubeMap Example

## 概述 (Overview)

这个例子演示了如何在ULRE中实现渲染到立方体贴图的技术。立方体贴图渲染是一种重要的3D图形技术，常用于环境映射、反射、阴影映射等效果。

This example demonstrates how to implement rendering to cube maps in ULRE. Cube map rendering is an important 3D graphics technique commonly used for environment mapping, reflections, shadow mapping, and other effects.

## 技术原理 (Technical Principles)

### 立方体贴图结构 (Cube Map Structure)
立方体贴图由6个面组成，分别对应：
- +X (右面, Right)
- -X (左面, Left) 
- +Y (上面, Top)
- -Y (下面, Bottom)
- +Z (前面, Front)
- -Z (后面, Back)

A cube map consists of 6 faces corresponding to:
- +X (Right face)
- -X (Left face)
- +Y (Top face)
- -Y (Bottom face)
- +Z (Front face)
- -Z (Back face)

### 摄像机设置 (Camera Setup)
对于每个立方体面，需要设置不同的摄像机方向：

For each cube face, different camera orientations are required:

```cpp
// 6个面的摄像机朝向和上向量
CubeFaceInfo face_info[6] = {
    { Vector3f(1, 0, 0), Vector3f(0, -1, 0) },   // +X
    { Vector3f(-1, 0, 0), Vector3f(0, -1, 0) },  // -X
    { Vector3f(0, 1, 0), Vector3f(0, 0, 1) },    // +Y
    { Vector3f(0, -1, 0), Vector3f(0, 0, -1) },  // -Y
    { Vector3f(0, 0, 1), Vector3f(0, -1, 0) },   // +Z
    { Vector3f(0, 0, -1), Vector3f(0, -1, 0) }   // -Z
};
```

每个摄像机都使用90度的视野角度（FOV），确保正确覆盖立方体的一个面。

Each camera uses a 90-degree field of view (FOV) to properly cover one face of the cube.

## 实现方法 (Implementation)

### 1. 渲染目标创建 (Render Target Creation)
为每个立方体面创建独立的渲染目标：

Create separate render targets for each cube face:

```cpp
FramebufferInfo fbi(UPF_RGBA8, CUBEMAP_SIZE, CUBEMAP_SIZE);
face.render_target = device->CreateRT(&fbi);
```

### 2. 场景渲染 (Scene Rendering)
对每个面都从中心点出发，朝着相应方向渲染场景：

For each face, render the scene from the center point looking in the corresponding direction:

```cpp
// 摄像机位置在原点
face.cam.pos = Vector4f(0, 0, 0, 1);
// 朝向对应的立方体面方向
face.cam.target = Vector4f(face_info[i].target, 1);
// 设置上向量
face.cam.up = Vector4f(face_info[i].up, 1);
```

### 3. 结果显示 (Result Display)
将渲染的立方体面纹理显示在一个立方体上进行验证：

Display the rendered cube face textures on a cube for verification:

```cpp
// 使用第一个面的纹理作为演示
display.material_instance->BindImageSampler(DescriptorSetType::Value, "tex",
                                           cube_faces[0].render_target->GetColorTexture(),
                                           display.sampler);
```

## 应用场景 (Use Cases)

1. **环境映射 (Environment Mapping)**: 为物体创建真实的环境反射
2. **动态天空盒 (Dynamic Skybox)**: 实时生成周围环境的天空盒
3. **阴影映射 (Shadow Mapping)**: 点光源的全向阴影
4. **预计算辐照 (Precomputed Irradiance)**: 基于物理的渲染中的环境光照

1. **Environment Mapping**: Create realistic environmental reflections for objects
2. **Dynamic Skybox**: Real-time generation of surrounding environment skyboxes
3. **Shadow Mapping**: Omnidirectional shadows for point lights
4. **Precomputed Irradiance**: Environmental lighting for physically-based rendering

## 扩展建议 (Extension Suggestions)

1. **真正的立方体贴图纹理**: 将6个2D纹理合并成真正的立方体贴图纹理
2. **动态更新**: 实现每帧更新立方体贴图以支持动态反射
3. **多层级**: 支持不同分辨率的立方体贴图用于不同用途
4. **优化**: 使用几何着色器或其他技术一次性渲染所有6个面

1. **Actual Cube Map Texture**: Combine the 6 2D textures into a proper cube map texture
2. **Dynamic Updates**: Update the cube map every frame to support dynamic reflections
3. **Multiple Levels**: Support different resolution cube maps for different purposes
4. **Optimization**: Use geometry shaders or other techniques to render all 6 faces at once

## 文件结构 (File Structure)

- `RenderToCubemap.cpp`: 主要实现文件，演示完整的立方体贴图渲染流程
- `RenderToCubemapSimple.cpp`: 简化版本，只渲染一个面作为概念验证

- `RenderToCubemap.cpp`: Main implementation file demonstrating complete cube map rendering workflow
- `RenderToCubemapSimple.cpp`: Simplified version rendering only one face as proof of concept

## 构建和运行 (Build and Run)

确保在CMakeLists.txt中启用了相应的编译选项：

Ensure the corresponding build options are enabled in CMakeLists.txt:

```cmake
CreateProject("Texture" RenderToCubemap RenderToCubemap.cpp)
```

然后使用标准的CMake构建流程编译和运行示例。

Then use the standard CMake build process to compile and run the example.