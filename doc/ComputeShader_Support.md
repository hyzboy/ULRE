# Compute Shader Support Implementation

## Overview
This implementation adds comprehensive Compute Shader support to the ULRE engine, enabling GPU-accelerated computation tasks.

## New Components

### 1. ShaderCreateInfoCompute Class
**Location:** `inc/hgl/shadergen/ShaderCreateInfoCompute.h`

A specialized class for creating compute shaders, inheriting from `ShaderCreateInfo`.

**Key Features:**
- Work group size configuration via `SetWorkGroupSize(x, y, z)`
- No traditional vertex/fragment input/output requirements
- Supports UBO/SSBO/Image descriptors for data exchange
- Automatic default work group size (1,1,1) if not specified

**Example Usage:**
```cpp
MaterialDescriptorInfo *mdi = new MaterialDescriptorInfo();
ShaderCreateInfoCompute *csci = new ShaderCreateInfoCompute(mdi);
csci->SetWorkGroupSize(256, 1, 1);  // Configure work group size
csci->SetMain(compute_main_code);    // Set shader main function
csci->CreateShader(nullptr);         // Compile to SPIR-V
```

### 2. ComputeShaderDescriptorInfo
**Location:** `inc/hgl/shadergen/ShaderDescriptorInfo.h`

Type alias based on `CustomShaderDescriptorInfo` template, handling compute shader descriptors.

```cpp
using ComputeShaderDescriptorInfo = 
    CustomShaderDescriptorInfo<ShaderStage::Compute, SVArray, ShaderVariable, SVArray, ShaderVariable>;
```

### 3. ComputePipeline Class
**Location:** `inc/hgl/graph/VKComputePipeline.h`

Manages compute shader pipelines, similar to graphics pipelines but simplified.

**Key Methods:**
- Constructor: Private, created via `VulkanDevice::CreateComputePipeline()`
- `GetName()`: Returns pipeline name
- `operator VkPipeline()`: Direct Vulkan pipeline access
- `GetPipelineLayout()`: Returns pipeline layout

### 4. VulkanDevice Compute Pipeline Creation
**Location:** `inc/hgl/graph/VKDevice.h`, `src/SceneGraph/Vulkan/VKDevice.cpp`

New method added to VulkanDevice:
```cpp
ComputePipeline *CreateComputePipeline(
    const AnsiString &name,
    VkShaderModule shader_module,
    VkPipelineLayout pipeline_layout
);
```

**Implementation Details:**
- Creates `VkComputePipelineCreateInfo` structure
- Uses pipeline cache for optimization
- Supports debug utilities for pipeline naming
- Returns `ComputePipeline` instance or nullptr on failure

## Example Test Cases

### Test 1: ComputeShaderBasicTest
**Location:** `example/Compute/ComputeShaderBasicTest.cpp`

Demonstrates:
- Compute shader API overview
- Work group configuration examples (1D, 2D, 3D)
- Sample compute shader code
- Complete usage workflow

### Test 2: ComputeShaderArrayAdd
**Location:** `example/Compute/ComputeShaderArrayAdd.cpp`

Demonstrates:
- Array addition using compute shaders
- SSBO (Shader Storage Buffer Object) usage
- Practical compute shader code example
- Workflow for data processing

## GLSL Compute Shader Example

```glsl
#version 450

// Work group size (automatically set by SetWorkGroupSize)
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// Input buffers
layout(std430, binding = 0) buffer InputA {
    float data_a[];
};

layout(std430, binding = 1) buffer InputB {
    float data_b[];
};

// Output buffer
layout(std430, binding = 2) buffer Output {
    float data_out[];
};

void main() {
    uint index = gl_GlobalInvocationID.x;
    data_out[index] = data_a[index] + data_b[index];
}
```

## Usage Workflow

1. **Create Descriptor Info:**
   ```cpp
   MaterialDescriptorInfo *mdi = new MaterialDescriptorInfo();
   ```

2. **Create Compute Shader Info:**
   ```cpp
   ShaderCreateInfoCompute *csci = new ShaderCreateInfoCompute(mdi);
   ```

3. **Configure Work Group:**
   ```cpp
   csci->SetWorkGroupSize(256, 1, 1);
   ```

4. **Add Descriptors:**
   ```cpp
   csci->AddSSBO(DescriptorSetType::Value, ssbo_descriptor);
   ```

5. **Set Shader Code:**
   ```cpp
   csci->SetMain(main_function_code);
   ```

6. **Compile Shader:**
   ```cpp
   csci->CreateShader(nullptr);
   ```

7. **Create Shader Module:**
   ```cpp
   VkShaderModule module = CreateShaderModule(
       device, 
       csci->GetSPVData(), 
       csci->GetSPVSize()
   );
   ```

8. **Create Compute Pipeline:**
   ```cpp
   ComputePipeline *pipeline = device->CreateComputePipeline(
       "MyComputePipeline",
       module,
       pipeline_layout
   );
   ```

9. **Use in Command Buffer:**
   ```cpp
   vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
   vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, ...);
   vkCmdDispatch(cmd_buf, num_groups_x, num_groups_y, num_groups_z);
   ```

## Work Group Size Guidelines

### 1D Data Processing
- Work group size: `(256, 1, 1)` or `(64, 1, 1)`
- Use case: Array operations, particle updates

### 2D Image Processing
- Work group size: `(16, 16, 1)` or `(8, 8, 1)`
- Use case: Image filters, post-processing effects

### 3D Volume Processing
- Work group size: `(8, 8, 8)` or `(4, 4, 4)`
- Use case: Volumetric rendering, 3D simulations

## Build Integration

All necessary files have been added to CMakeLists.txt:
- `src/ShaderGen/CMakeLists.txt`: Added `ShaderCreateInfoCompute.cpp`
- `src/SceneGraph/CMakeLists.txt`: Added `VKComputePipeline.cpp`
- `example/CMakeLists.txt`: Added Compute subdirectory

## Testing

Build and run the test examples:
```bash
# Build the project
cmake --preset linux-gcc-debug
cmake --build build/linux-gcc-debug

# Run tests
./build/linux-gcc-debug/ComputeShaderBasicTest
./build/linux-gcc-debug/ComputeShaderArrayAdd
```

## Future Enhancements

Potential improvements for future iterations:
1. Add support for push constants in compute shaders
2. Implement compute queue management
3. Add helper functions for common compute patterns
4. Support for indirect dispatch
5. Integration with existing material system
6. Performance profiling tools for compute workloads

## Notes

- All compute shader features are consistent with Vulkan 1.0+ compute capabilities
- The implementation follows the existing ULRE engine architecture patterns
- Compute shaders share descriptor set management with graphics shaders
- Pipeline cache is automatically used for faster pipeline creation
