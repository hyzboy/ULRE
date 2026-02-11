# Compute Shader Examples

This directory contains example programs demonstrating the usage of Compute Shader support in ULRE.

## Examples

### 1. ComputeShaderBasicTest
**Purpose:** Introduction to Compute Shader API and concepts

This example demonstrates:
- Overview of Compute Shader capabilities
- Work group size configuration for 1D, 2D, and 3D data
- Basic GLSL compute shader syntax
- Complete workflow from creation to execution

**Usage:**
```bash
./ComputeShaderBasicTest
```

**Output:** Prints comprehensive information about:
- Available compute shader API
- Work group configuration examples
- Sample GLSL compute shader code
- Step-by-step usage workflow

### 2. ComputeShaderArrayAdd
**Purpose:** Practical example of array computation using compute shaders

This example demonstrates:
- Using compute shaders for parallel array operations
- SSBO (Shader Storage Buffer Object) configuration
- Work group dispatch calculation
- Real-world compute shader application

**Usage:**
```bash
./ComputeShaderArrayAdd
```

**Output:** Shows:
- Sample compute shader code for array addition
- Detailed workflow explanation
- Expected results and performance considerations
- API usage patterns

## Running the Examples

### Prerequisites
- Vulkan SDK installed
- ULRE engine built successfully
- GPU with compute shader support

### Build
```bash
cd ULRE
cmake --preset linux-gcc-debug  # or appropriate preset for your system
cmake --build build/linux-gcc-debug
```

### Execute
```bash
cd build/linux-gcc-debug
./ComputeShaderBasicTest
./ComputeShaderArrayAdd
```

## Learning Path

1. **Start with ComputeShaderBasicTest** to understand the API and workflow
2. **Study ComputeShaderArrayAdd** for a practical implementation example
3. **Review the documentation** in `doc/ComputeShader_Support.md`
4. **Implement your own** compute shader for specific use cases

## Common Use Cases

### Data Processing
- Array operations (addition, multiplication, etc.)
- Particle system updates
- Physics simulations
- Data transformations

### Image Processing
- Filters (blur, sharpen, edge detection)
- Color adjustments
- Image scaling and resampling
- Computer vision algorithms

### Scientific Computing
- Matrix operations
- FFT (Fast Fourier Transform)
- Numerical simulations
- Ray tracing

## Additional Resources

- **API Documentation:** `doc/ComputeShader_Support.md` (English)
- **中文文档:** `doc/ComputeShader_Support_CN.md` (Chinese)
- **GLSL Compute Shader Specification:** [Khronos GLSL Specification](https://www.khronos.org/opengl/wiki/Compute_Shader)
- **Vulkan Compute Tutorial:** [Vulkan Tutorial - Compute Shader](https://vulkan-tutorial.com/)

## Notes

- These examples demonstrate the API structure but require full Vulkan device initialization for actual execution
- Work group sizes should be tuned based on GPU architecture for optimal performance
- Always ensure proper synchronization when using compute shaders with graphics pipelines
