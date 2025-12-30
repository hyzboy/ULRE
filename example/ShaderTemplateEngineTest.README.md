# Shader Template Engine Test

This example demonstrates and validates the ShaderTemplateEngine system.

## Building

The test is included in the example suite. Build it with:

```bash
mkdir build
cd build
cmake ..
make ShaderTemplateEngineTest
```

## Running

From the ULRE root directory:

```bash
./build/example/ShaderTemplateEngineTest ./
```

Or from the build directory:

```bash
cd build/example
./ShaderTemplateEngineTest ../../
```

## What it Tests

The test suite validates four key aspects of the ShaderTemplateEngine:

### Test 1: Module Loading
- Loads shader modules from the library
- Verifies module metadata (provides, dependencies)
- Tests caching mechanism

### Test 2: Dependency Resolution
- Creates a recipe with multiple modules
- Resolves dependencies between modules
- Validates topological sorting

### Test 3: Recipe Loading
- Loads a material recipe from JSON
- Parses quality levels
- Validates module mappings

### Test 4: Shader Code Generation
- Generates complete GLSL shader code
- Combines multiple modules
- Applies template rendering

## Expected Output

When all tests pass, you should see output similar to:

```
Shader Template Engine Test Suite
==================================

Configuration:
  - Templates: ./ShaderLibrary/templates
  - Modules: ./ShaderLibrary/modules
  - Recipe: ./ShaderLibrary/recipes/standard/metal.json

================================================================================

TEST 1: Module Loading
-----------------------
SUCCESS: Loaded lambert module
  - Name: lambert
  - Provides: GetDiffuseColor
  - Dependencies: GetAlbedo
SUCCESS: Loaded texture_albedo module

================================================================================

TEST 2: Dependency Resolution
------------------------------
SUCCESS: Dependencies resolved
Module load order:
  1. albedo/texture_albedo
  2. lighting/lambert
  3. specular/phong
  4. ambient/skylight

================================================================================

TEST 3: Recipe Loading
----------------------
SUCCESS: Loaded recipe
  - Name: Metal
  - Template: forward.frag.tmpl
  - Modules:
      lighting -> lambert
      specular -> phong
      ambient -> skylight
      albedo -> texture_albedo

================================================================================

TEST 4: Shader Code Generation
-------------------------------
SUCCESS: Generated shader code (1234 bytes)

Generated GLSL (first 500 chars):
----------------------------------------
#version 450

// ========== Fragment Inputs ==========
layout(location=0) in vec3 Normal;
layout(location=1) in vec2 TexCoord;
...

================================================================================

Test Summary
============
ALL TESTS PASSED ✓
```

## Troubleshooting

If tests fail, check:

1. **Module loading fails**: Verify ShaderLibrary directory structure exists
2. **Dependency resolution fails**: Check interface JSON files are valid
3. **Recipe loading fails**: Validate recipe JSON syntax
4. **Code generation fails**: Review template syntax and module code

## Integration with Build System

To integrate this test into your build:

1. Add to example/CMakeLists.txt:
```cmake
add_executable(ShaderTemplateEngineTest ShaderTemplateEngineTest.cpp)
target_link_libraries(ShaderTemplateEngineTest ${ULRE})
```

2. Run as part of automated testing:
```bash
ctest --test-dir build -R ShaderTemplateEngineTest
```
