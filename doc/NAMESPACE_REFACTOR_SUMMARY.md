# ShaderGen/SceneGraph Namespace Decoupling Summary

## Overview
This document summarizes the namespace refactoring work completed to decouple shader_schema from the hgl::graph namespace, moving all shader_schema types into their proper hgl::shader_schema namespace while maintaining backward compatibility.

## Changes Made

### 1. VK_NAMESPACE Macro Update
**File**: `inc/hgl/shader_schema/VkTypes.h`

The VK_NAMESPACE macro was changed from:
```cpp
#define VK_NAMESPACE        hgl::graph
#define VK_NAMESPACE_BEGIN  namespace hgl::graph{
#define VK_NAMESPACE_END    }
```

To:
```cpp
#define VK_NAMESPACE        hgl::shader_schema
#define VK_NAMESPACE_BEGIN  namespace hgl::shader_schema{
#define VK_NAMESPACE_END    }
```

This change affects all files that use VK_NAMESPACE_BEGIN/END macros, automatically moving their content into the hgl::shader_schema namespace.

### 2. ShaderStageHelpers Functions Migration
**New File**: `src/ShaderSchema/ShaderStageHelpers.cpp`
**Updated File**: `src/SceneGraph/Vulkan/VKShaderStage.cpp`

Functions moved from hgl::graph to hgl::shader_schema:
- `GetShaderCountByBits()`
- `GetMaxShaderStage()`
- `GetShaderStageName()`
- `GetShaderStageFlagBits()`
- `GetVulkanFormat()` (for VertexInputAttribute)

The SceneGraph implementation now just includes the header for backward compatibility.

### 3. Namespace Changes for Core Types

The following types were moved from `hgl::graph` to `hgl::shader_schema`:

#### Basic Types
- `DescriptorSetType` - Descriptor set type enumeration
- `PrimitiveType` - Primitive type enumeration
- `CoordinateSystem2D` - 2D coordinate system types
- `Interpolation` - Interpolation types for shaders

#### Shader Types
- `ShaderStage` - Shader stage enumeration
- `ShaderDescriptor` and related types (UBODescriptor, SSBODescriptor, etc.)
- `ShaderDescriptorSet` - Descriptor set management
- `ShaderVariableType` - Shader variable type system
- `ShaderBufferSource` - Shader buffer source definitions

#### Material Types
- `StdMaterial` - Standard material base class
- `MaterialCreateConfig` - Material configuration
- `Material2DCreateConfig` - 2D material configuration
- `Material3DCreateConfig` - 3D material configuration
- `MaterialLibrary` - Material library system

#### Vulkan Types
- `VulkanPhyDevice` - Physical device wrapper
- `VulkanFormat` - Format utilities
- `VulkanDevAttr` - Device attributes
- `RenderTargetOutputConfig` - Render target configuration

#### Sampler and Texture Types
- `SamplerType` - Sampler type enumeration
- `SamplerName` - Sampler name constants
- `ShaderImageType` - Shader image types
- `TextureType` - Texture type enumeration

#### Vertex Types
- `VertexInputGroup` - Vertex input grouping
- `VertexAttribType` - Vertex attribute types
- `VertexInputAttribute` - Vertex input attributes

### 4. STD_MTL_NAMESPACE Update
**File**: `inc/hgl/shader_schema/StdMaterial.h`

The STD_MTL_NAMESPACE macro was changed from:
```cpp
#define STD_MTL_NAMESPACE hgl::graph::mtl
```

To:
```cpp
#define STD_MTL_NAMESPACE hgl::shader_schema::mtl
```

This affects all material-related types and functions.

### 5. Backward Compatibility Aliases

Every moved type now has a corresponding `using` declaration in the `hgl::graph` namespace to maintain backward compatibility. For example:

```cpp
namespace hgl::shader_schema
{
    enum class DescriptorSetType { /* ... */ };
}

// Backward compatibility
namespace hgl::graph
{
    using hgl::shader_schema::DescriptorSetType;
}
```

This pattern was applied to all moved types, ensuring that existing code using `hgl::graph::DescriptorSetType` continues to work without modification.

## File Changes Summary

### Headers Modified with Namespace Changes
1. `inc/hgl/shader_schema/VkTypes.h` - VK_NAMESPACE redefinition
2. `inc/hgl/shader_schema/ShaderStageHelpers.h` - Function declarations
3. `inc/hgl/shader_schema/DescriptorSetType.h` - Enum and functions
4. `inc/hgl/shader_schema/ShaderDescriptor.h` - Descriptor types
5. `inc/hgl/shader_schema/PrimitiveType.h` - Primitive types
6. `inc/hgl/shader_schema/CoordinateSystem.h` - Coordinate systems
7. `inc/hgl/shader_schema/ShaderBufferSource.h` - Buffer sources
8. `inc/hgl/shader_schema/SamplerName.h` - Sampler names
9. `inc/hgl/shader_schema/RenderTargetOutputConfig.h` - Render target config
10. `inc/hgl/shader_schema/StdMaterial.h` - Material base class

### Headers with Compatibility Aliases Added
All the above headers plus:
- `Interpolation.h`, `ShaderStage.h`
- `RenderAssign.h`, `ShaderDescriptorSet.h`
- `ShaderVariableType.h`, `PhysicalDevice.h`
- `VkFormat.h`, `VkStruct.h`, `VulkanDevAttr.h`
- `SamplerType.h`, `ShaderImageType.h`, `TextureType.h`
- `MaterialLibrary.h`, `MaterialCreateConfig.h`
- `Material2DCreateConfig.h`, `Material3DCreateConfig.h`
- `UBOCommon.h`, `BlinnPhong.h`

### Implementation Files Modified
1. `src/ShaderSchema/ShaderStageHelpers.cpp` - New implementation file
2. `src/ShaderSchema/CMakeLists.txt` - Added new source file
3. `src/SceneGraph/Vulkan/VKShaderStage.cpp` - Simplified to use shader_schema

## Benefits

1. **Clear Separation**: shader_schema is now clearly in its own namespace, making it independent of graph
2. **Backward Compatibility**: All existing code continues to work without changes
3. **Better Organization**: Types are now in their appropriate namespace based on their function
4. **Easier Maintenance**: Future changes to shader_schema won't affect graph namespace directly
5. **Clearer Dependencies**: The dependency hierarchy is now more explicit

## Migration Path for Users

### No Immediate Action Required
All existing code will continue to work due to backward compatibility aliases.

### Optional Migration
Users can gradually update their code to use the new namespace:

```cpp
// Old code (still works)
using namespace hgl::graph;
DescriptorSetType type = DescriptorSetType::Global;

// New code (recommended)
using namespace hgl::shader_schema;
DescriptorSetType type = DescriptorSetType::Global;
```

## Testing Recommendations

1. Build the ShaderSchema module independently to verify it has no graph dependencies
2. Build the ShaderGen module to ensure it works with the new namespace
3. Build the SceneGraph module to verify backward compatibility
4. Run existing tests to ensure functionality is preserved
5. Check for any compilation warnings related to namespace usage

## Future Work

1. Gradually update internal code to use hgl::shader_schema directly
2. Consider deprecating the hgl::graph aliases in a future major version
3. Update documentation to reflect the new namespace structure
4. Add explicit tests for namespace separation

## Conclusion

The namespace refactoring successfully decouples shader_schema from hgl::graph while maintaining complete backward compatibility. This sets the foundation for better code organization and clearer module boundaries in the ULRE project.
