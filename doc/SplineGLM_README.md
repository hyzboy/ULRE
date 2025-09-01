# 3D Spline Curve Generator for Z-up Coordinate System

This implementation provides a comprehensive 3D spline curve generator designed specifically for Z-up coordinate systems. It extends the existing template-based Spline class with GLM vec3 support and convenience functions.

## Features

- **GLM Integration**: Uses glm::vec3 for 3D coordinates
- **Z-up Coordinate System**: Designed for 3D graphics applications where Z is the up axis
- **Unlimited Control Points**: Can handle any number of control points (minimum based on spline order)
- **Multiple Spline Types**: Supports UNIFORM and OPEN_UNIFORM nodal vector types
- **Robust API**: Easy-to-use wrapper around the template-based Spline class

## Basic Usage

```cpp
#include <hgl/graph/SplineGLM.h>

// Create a 3D spline curve (order 3, cubic spline)
hgl::graph::SplineCurve3D curve(3, hgl::graph::SplineNode::OPEN_UNIFORM);

// Add control points
std::vector<glm::vec3> points = {
    glm::vec3(0.0f, 0.0f, 0.0f),    // Start
    glm::vec3(1.0f, 0.0f, 1.0f),    // Up and forward
    glm::vec3(2.0f, 1.0f, 2.0f),    // Right, up and forward
    glm::vec3(3.0f, 0.0f, 1.0f),    // Forward and slightly up
    glm::vec3(4.0f, 0.0f, 0.0f)     // End at ground level
};
curve.SetControlPoints(points);

// Evaluate the curve
glm::vec3 position = curve.Evaluate(0.5f);      // Position at t=0.5
glm::vec3 tangent = curve.GetTangent(0.5f);     // Normalized tangent

// Generate points along the curve
auto interpolated_points = curve.GeneratePoints(20);
```

## Interactive Building

```cpp
// Start with an empty curve
hgl::graph::SplineCurve3D curve;

// Add points one by one
curve.AddControlPoint(glm::vec3(0.0f, 0.0f, 0.0f));
curve.AddControlPoint(glm::vec3(1.0f, 1.0f, 1.0f));
curve.AddControlPoint(glm::vec3(2.0f, 0.0f, 2.0f));

// Check if curve is valid for evaluation
if (curve.IsValid()) {
    glm::vec3 midpoint = curve.Evaluate(0.5f);
}
```

## Utility Functions

The library provides utility functions for common Z-up coordinate system shapes:

```cpp
// Create a circle in the XY plane at Z=1.0
auto circle = hgl::graph::SplineUtils::CreateCircle(
    glm::vec3(0.0f, 0.0f, 1.0f),  // center
    2.0f,                          // radius
    8                              // control points
);

// Create a helical curve around the Z axis
auto helix = hgl::graph::SplineUtils::CreateHelix(
    glm::vec3(0.0f, 0.0f, 0.0f),  // base center
    1.0f,                          // radius
    5.0f,                          // height
    2.0f,                          // turns
    6                              // points per turn
);
```

## Advanced Features

- **Arc Length Calculation**: Approximate the length of the curve
- **Tangent Generation**: Get normalized tangent vectors for orientation
- **Batch Point Generation**: Generate points with tangent vectors
- **Flexible Node Types**: Choose between UNIFORM and OPEN_UNIFORM spacing

## Coordinate System Notes

This implementation is designed for Z-up coordinate systems where:
- X and Y form the horizontal plane
- Z is the vertical (up) axis
- Default tangent vectors point in the +Z direction when no motion is detected

This is commonly used in:
- 3D modeling applications
- Game engines with Z-up conventions
- CAD systems
- Scientific visualization