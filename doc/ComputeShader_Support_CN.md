# Compute Shader 支持实现

## 概述
本实现为 ULRE 引擎添加了全面的 Compute Shader（计算着色器）支持，支持 GPU 加速的计算任务。

## 新增组件

### 1. ShaderCreateInfoCompute 类
**位置:** `inc/hgl/shadergen/ShaderCreateInfoCompute.h`

用于创建计算着色器的专用类，继承自 `ShaderCreateInfo`。

**主要功能:**
- 通过 `SetWorkGroupSize(x, y, z)` 配置工作组大小
- 不需要传统的顶点/片段着色器的输入输出
- 支持 UBO/SSBO/Image 描述符进行数据交换
- 如果未指定，自动使用默认工作组大小 (1,1,1)

**使用示例:**
```cpp
MaterialDescriptorInfo *mdi = new MaterialDescriptorInfo();
ShaderCreateInfoCompute *csci = new ShaderCreateInfoCompute(mdi);
csci->SetWorkGroupSize(256, 1, 1);  // 配置工作组大小
csci->SetMain(compute_main_code);    // 设置着色器主函数
csci->CreateShader(nullptr);         // 编译为 SPIR-V
```

### 2. ComputeShaderDescriptorInfo
**位置:** `inc/hgl/shadergen/ShaderDescriptorInfo.h`

基于 `CustomShaderDescriptorInfo` 模板的类型别名，用于处理计算着色器描述符。

```cpp
using ComputeShaderDescriptorInfo = 
    CustomShaderDescriptorInfo<ShaderStage::Compute, SVArray, ShaderVariable, SVArray, ShaderVariable>;
```

### 3. ComputePipeline 类
**位置:** `inc/hgl/graph/VKComputePipeline.h`

管理计算着色器管线，类似于图形管线但更简化。

**主要方法:**
- 构造函数: 私有，通过 `VulkanDevice::CreateComputePipeline()` 创建
- `GetName()`: 返回管线名称
- `operator VkPipeline()`: 直接访问 Vulkan 管线
- `GetPipelineLayout()`: 返回管线布局

### 4. VulkanDevice 计算管线创建
**位置:** `inc/hgl/graph/VKDevice.h`, `src/SceneGraph/Vulkan/VKDevice.cpp`

VulkanDevice 新增方法:
```cpp
ComputePipeline *CreateComputePipeline(
    const AnsiString &name,
    VkShaderModule shader_module,
    VkPipelineLayout pipeline_layout
);
```

**实现细节:**
- 创建 `VkComputePipelineCreateInfo` 结构
- 使用管线缓存进行优化
- 支持调试工具进行管线命名
- 返回 `ComputePipeline` 实例，失败返回 nullptr

## 示例测试用例

### 测试 1: ComputeShaderBasicTest
**位置:** `example/Compute/ComputeShaderBasicTest.cpp`

展示:
- 计算着色器 API 概览
- 工作组配置示例（1D、2D、3D）
- 示例计算着色器代码
- 完整的使用流程

### 测试 2: ComputeShaderArrayAdd
**位置:** `example/Compute/ComputeShaderArrayAdd.cpp`

展示:
- 使用计算着色器进行数组加法
- SSBO（着色器存储缓冲对象）使用
- 实用的计算着色器代码示例
- 数据处理工作流

## GLSL 计算着色器示例

```glsl
#version 450

// 工作组大小（由 SetWorkGroupSize 自动设置）
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// 输入缓冲
layout(std430, binding = 0) buffer InputA {
    float data_a[];
};

layout(std430, binding = 1) buffer InputB {
    float data_b[];
};

// 输出缓冲
layout(std430, binding = 2) buffer Output {
    float data_out[];
};

void main() {
    uint index = gl_GlobalInvocationID.x;
    data_out[index] = data_a[index] + data_b[index];
}
```

## 使用流程

1. **创建描述符信息:**
   ```cpp
   MaterialDescriptorInfo *mdi = new MaterialDescriptorInfo();
   ```

2. **创建计算着色器信息:**
   ```cpp
   ShaderCreateInfoCompute *csci = new ShaderCreateInfoCompute(mdi);
   ```

3. **配置工作组:**
   ```cpp
   csci->SetWorkGroupSize(256, 1, 1);
   ```

4. **添加描述符:**
   ```cpp
   csci->AddSSBO(DescriptorSetType::Value, ssbo_descriptor);
   ```

5. **设置着色器代码:**
   ```cpp
   csci->SetMain(main_function_code);
   ```

6. **编译着色器:**
   ```cpp
   csci->CreateShader(nullptr);
   ```

7. **创建着色器模块:**
   ```cpp
   VkShaderModule module = CreateShaderModule(
       device, 
       csci->GetSPVData(), 
       csci->GetSPVSize()
   );
   ```

8. **创建计算管线:**
   ```cpp
   ComputePipeline *pipeline = device->CreateComputePipeline(
       "MyComputePipeline",
       module,
       pipeline_layout
   );
   ```

9. **在命令缓冲中使用:**
   ```cpp
   vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
   vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, ...);
   vkCmdDispatch(cmd_buf, num_groups_x, num_groups_y, num_groups_z);
   ```

## 工作组大小指南

### 1D 数据处理
- 工作组大小: `(256, 1, 1)` 或 `(64, 1, 1)`
- 使用场景: 数组操作、粒子更新

### 2D 图像处理
- 工作组大小: `(16, 16, 1)` 或 `(8, 8, 1)`
- 使用场景: 图像滤镜、后处理效果

### 3D 体积处理
- 工作组大小: `(8, 8, 8)` 或 `(4, 4, 4)`
- 使用场景: 体积渲染、3D 模拟

## 构建集成

所有必要的文件已添加到 CMakeLists.txt:
- `src/ShaderGen/CMakeLists.txt`: 添加了 `ShaderCreateInfoCompute.cpp`
- `src/SceneGraph/CMakeLists.txt`: 添加了 `VKComputePipeline.cpp`
- `example/CMakeLists.txt`: 添加了 Compute 子目录

## 测试

构建并运行测试示例:
```bash
# 构建项目
cmake --preset linux-gcc-debug
cmake --build build/linux-gcc-debug

# 运行测试
./build/linux-gcc-debug/ComputeShaderBasicTest
./build/linux-gcc-debug/ComputeShaderArrayAdd
```

## 未来改进

未来迭代的潜在改进:
1. 添加对计算着色器中 push constants 的支持
2. 实现计算队列管理
3. 添加常见计算模式的辅助函数
4. 支持间接调度
5. 与现有材质系统集成
6. 计算工作负载的性能分析工具

## 注意事项

- 所有计算着色器功能与 Vulkan 1.0+ 计算能力一致
- 实现遵循现有 ULRE 引擎架构模式
- 计算着色器与图形着色器共享描述符集管理
- 自动使用管线缓存以加快管线创建速度
