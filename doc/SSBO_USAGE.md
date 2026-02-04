# SSBO (Storage Buffer Object) Usage Guide

## Overview

SSBOs (Storage Buffer Objects) are now fully supported in the ULRE ShaderGen system. SSBOs provide:
- Dynamic-size arrays in shaders (vs UBO's fixed-size arrays)
- Large data storage beyond UBO limits (~64KB)
- Read/write access from shaders (unlike read-only UBOs)

## Version Compatibility

**Current Implementation**: Compatible with devel_49_ecs_input branch
- Uses CMCore container types (`UnorderedMap<>` from `hgl/type/UnorderedMap.h`)
- Migrated from custom `Map<>` template to UnorderedMap
- Aligns with CMCore's container abstraction layer

## Implementation

The following components have been added to support SSBOs:

### 1. Core Descriptor Structure
**File**: `inc/hgl/graph/VKShaderDescriptor.h`

```cpp
struct SSBODescriptor : public ShaderDescriptor
{
    AnsiString type;
    
public:
    SSBODescriptor()
    {
        desc_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
};
```

### 2. Shader Descriptor Management
**File**: `inc/hgl/shadergen/ShaderDescriptorInfo.h`

- `SSBODescriptorList` type definition
- `ssbo_list` member variable
- `GetSSBOList()` accessor method
- `AddSSBO()` method

### 3. GLSL Code Generation
**File**: `src/ShaderGen/ShaderCreateInfo.cpp`

The `ProcSSBO()` function generates GLSL buffer declarations:

```glsl
layout(set=X, binding=Y) buffer StructName
{
    // struct members here
} instanceName;
```

**Key difference from UBO**: Uses `buffer` keyword instead of `uniform`

### 4. Material API
**File**: `inc/hgl/shadergen/MaterialCreateInfo.h`

Three new methods for adding SSBOs:
- `AddSSBO(ShaderStage, DescriptorSetType, struct_name, name)` - Single shader stage
- `AddSSBO(uint32_t flag_bits, DescriptorSetType, struct_name, name)` - Multiple stages
- `AddSSBOStruct(uint32_t flag_bits, ShaderBufferSource)` - With struct definition

## Usage Example

### C++ Code

```cpp
// 1. Define struct
const char* structCode = R"(
    mat4 matrices[];  // Dynamic array - only possible with SSBO!
)";

// 2. Create material
MaterialCreateInfo mci(...);
mci.AddStruct("TransformBuffer", structCode);

// 3. Add SSBO to vertex shader
mci.AddSSBO(
    ShaderStage::Vertex,
    DescriptorSetType::PerFrame,
    "TransformBuffer",
    "transforms"
);

// 4. At runtime, create and bind SSBO
DeviceBuffer* ssbo = device->CreateSSBO(sizeof(Matrix4f) * 10000);
material->BindSSBO(DescriptorSetType::PerFrame, "transforms", ssbo);
```

### Generated GLSL

```glsl
layout(set=0, binding=0) buffer TransformBuffer
{
    mat4 matrices[];
} transforms;

void main()
{
    mat4 transform = transforms.matrices[gl_InstanceIndex];
    gl_Position = transform * vec4(position, 1.0);
}
```

## Benefits Over UBO

| Feature | UBO | SSBO |
|---------|-----|------|
| Max Size | ~64KB | GPU dependent (often 128MB+) |
| Array Size | Fixed at compile time | Dynamic (runtime-sized arrays) |
| Access | Read-only | Read/Write |
| Performance | Faster for small data | Better for large data |

## Use Cases

### 1. Large Transform Arrays
For rendering thousands of objects with individual transforms:

```cpp
// SSBO can handle 10,000+ transforms
mci.AddSSBO(ShaderStage::Vertex, DescriptorSetType::PerFrame, 
            "TransformBuffer", "l2w");
```

### 2. Particle Systems
For particle data that exceeds UBO limits:

```cpp
mci.AddSSBO(ShaderStage::Vertex | ShaderStage::Fragment,
            DescriptorSetType::PerMaterial,
            "ParticleData", "particles");
```

### 3. Skinning/Animation
For skeletal animation with many bones:

```cpp
mci.AddSSBO(ShaderStage::Vertex, DescriptorSetType::PerObject,
            "BoneMatrices", "bones");
```

## Migration from UBO

To convert an existing UBO to SSBO:

1. Change `AddUBO()` call to `AddSSBO()`
2. Change `CreateUBO()` to `CreateSSBO()` at device level
3. Update shader struct to use dynamic arrays if needed:
   - UBO: `mat4 matrices[256];` (fixed size)
   - SSBO: `mat4 matrices[];` (dynamic size)

## Testing

The implementation has been verified through:
- ✅ Code structure matches existing UBO pattern
- ✅ Proper GLSL `buffer` keyword generation
- ✅ Descriptor set/binding assignment
- ✅ Integration with existing material system

## Technical Details

### Descriptor Type
SSBOs use `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` (defined in Vulkan spec)

### Processing Order
In shader generation pipeline:
1. ProcDefine()
2. ProcLayout()
3. ProcInput()
4. ProcMI()
5. ProcUBO()
6. **ProcSSBO()** ← New
7. ProcConstantID()
8. ProcSampler()
9. ProcOutput()

### Memory Layout
SSBOs follow `std430` layout rules by default in GLSL, which is more efficient than UBO's `std140`.

## Future Enhancements

Potential future additions:
- Compute shader SSBO support
- Atomic operations on SSBO data
- SSBO size query/validation helpers
- Performance profiling tools

## Implementation Notes

### Container Template Migration (devel_49_ecs_input)

The implementation uses CMCore container types for consistency with the framework:

**Container Changes:**
- `Map<K,V>` → `UnorderedMap<K,V>`
- `#include<hgl/type/Map.h>` → `#include<hgl/type/UnorderedMap.h>`

**Affected Files:**
- `inc/hgl/shadergen/MaterialDescriptorInfo.h`
- `src/ShaderGen/MaterialDescriptorInfo.cpp`

**API Usage:**
```cpp
// CMCore UnorderedMap (current)
#include<hgl/type/UnorderedMap.h>

UnorderedMap<K,V> map;
map[key] = value;
auto it = map.find(key);
if(it != map.end()) { out_value = it->second; }
bool exists = map.count(key) > 0;
```

**Why UnorderedMap from CMCore?**
- Provides abstraction over standard library containers
- Consistent with other CMCore type patterns (Map, ArrayList, etc.)
- Allows for custom allocators and optimizations
- Matches the framework's coding standards

This migration aligns with CMCore's container abstraction layer which provides consistent interfaces across the entire ULRE framework.
