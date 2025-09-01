# Capsule Geometry Implementation

This document describes the implementation of capsule (pill-shaped) geometry generators for the ULRE engine.

## Overview

Two new functions have been added to the InlineGeometry system:

1. `CreateCapsule` - Creates a solid capsule geometry (triangles)
2. `CreateCapsuleWireframe` - Creates a wireframe capsule geometry (lines)

## Structure

### CapsuleCreateInfo

```cpp
struct CapsuleCreateInfo
{
    float   halfExtend;     // 圆柱部分高度的一半 (half-height of cylindrical section)
    float   radius;         // 半径（圆柱和球帽共用） (radius for both cylinder and caps)
    uint    numberSlices;   // 圆切分精度 (circular subdivision count)
    uint    numberStacks;   // 球帽层数（半球的纬度分割数） (hemisphere stack count)
};
```

## Geometry Description

A capsule consists of three parts:
1. **Top Hemisphere**: From Z = halfExtend to Z = (halfExtend + radius)
2. **Cylindrical Section**: From Z = -halfExtend to Z = +halfExtend  
3. **Bottom Hemisphere**: From Z = -(halfExtend + radius) to Z = -halfExtend

## Coordinate System

- **Z-axis**: Points upward (positive Z is up)
- **Winding Order**: Clockwise when viewed from outside (front-facing)
- **Origin**: Center of the capsule at (0, 0, 0)

## Function Signatures

```cpp
// Solid capsule (triangles)
Primitive *CreateCapsule(PrimitiveCreater *pc, const CapsuleCreateInfo *cci);

// Wireframe capsule (lines)  
Primitive *CreateCapsuleWireframe(PrimitiveCreater *pc, const CapsuleCreateInfo *cci);
```

## Usage Example

```cpp
using namespace hgl::graph::inline_geometry;

// Create capsule parameters
CapsuleCreateInfo cci;
cci.halfExtend = 2.0f;      // Cylinder height = 4.0 units total
cci.radius = 1.0f;          // Radius = 1.0 unit
cci.numberSlices = 16;      // 16 subdivisions around circumference
cci.numberStacks = 8;       // 8 subdivisions per hemisphere

// Create solid capsule
auto pc = GetPrimitiveCreater(material_instance);
Primitive *capsule = CreateCapsule(pc, &cci);

// Create wireframe capsule
Primitive *wireframe = CreateCapsuleWireframe(pc, &cci);
```

## Implementation Details

### Vertex Generation
- Hemispheres use spherical coordinates (latitude/longitude)
- Cylinder uses circular cross-sections
- Proper normal vectors calculated for lighting
- UV texture coordinates provided
- Tangent vectors for normal mapping

### Index Generation
- Triangular tessellation for solid version
- Line segments for wireframe version
- Proper connectivity between hemisphere and cylinder sections
- Optimized for GPU rendering

### Coordinate System Compliance
- Matches existing ULRE geometry conventions
- Compatible with sphere and cylinder implementations
- Z-axis up orientation
- Clockwise front-face winding

## Test Application

A test application `CapsuleTest.cpp` has been created in `example/Basic/` to demonstrate usage of both solid and wireframe capsule geometries.

## Files Modified

1. `inc/hgl/graph/InlineGeometry.h` - Added struct and function declarations
2. `src/SceneGraph/InlineGeometry.cpp` - Added implementation
3. `example/Basic/CapsuleTest.cpp` - Added test application
4. `example/Basic/CMakeLists.txt` - Added test to build system