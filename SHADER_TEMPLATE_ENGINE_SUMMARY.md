# Shader Template Engine - Implementation Summary

This document provides an overview of the complete Shader Template Engine implementation.

## What Was Implemented

A modern, template-based shader generation system using the inja templating engine to replace traditional C++ string concatenation. The system provides:

- **Modular Design**: Each lighting model, specular model, and shader component is a separate GLSL file
- **Declarative Configuration**: JSON-based material recipes describe how to combine modules
- **Automatic Dependency Resolution**: Topological sorting ensures correct module loading order
- **Easy Extension**: Add new lighting models by creating a single GLSL file and interface definition
- **User-Friendly**: Pre-configured material recipes for common use cases

## Files Added

### Core Engine (3 files)
- `inc/hgl/shadergen/ShaderTemplateEngine.h` - Header with core classes
- `src/ShaderGen/ShaderTemplateEngine.cpp` - Implementation (430 lines)
- `src/ShaderGen/CMakeLists.txt` - Updated build configuration

### Shader Library (30 files)

#### Templates (2 files)
- `ShaderLibrary/templates/forward.vert.tmpl` - Vertex shader template
- `ShaderLibrary/templates/forward.frag.tmpl` - Fragment shader template

#### Modules (23 files in 6 categories)
- **Lighting** (5 files): lambert, half_lambert, blinn_phong, pbr_standard + interface
- **Specular** (4 files): phong, blinn_phong, ggx + interface
- **Ambient** (5 files): skylight, ibl_simple, ibl, sh + interface
- **Albedo** (4 files): texture_albedo, vertex_color, constant_color + interface
- **Normal** (3 files): vertex_normal, normal_map + interface
- **Utils** (2 files): common.glsl, pbr_functions.glsl

#### Recipes (5 files)
- `recipes/standard/metal.json` - Metallic material
- `recipes/standard/wood.json` - Wood material
- `recipes/standard/plastic.json` - Plastic material
- `recipes/organic/skin.json` - Skin material with SSS approximation
- `recipes/README.md` - Recipe documentation

### Documentation (3 files)
- `doc/ShaderTemplateEngine.md` - Complete system documentation (539 lines)
- `example/ShaderTemplateEngineTest.cpp` - Test suite (258 lines)
- `example/ShaderTemplateEngineTest.README.md` - Test documentation

### Dependencies (1 submodule)
- `3rdpty/inja` - Inja template engine with nlohmann/json

## Architecture Overview

```
User Application
       ↓
ShaderTemplateEngine
       ↓
    ┌──────────────┬──────────────┬──────────────┐
    ↓              ↓              ↓              ↓
Recipe Loading  Module Cache  Dependency    Template
                             Resolution     Rendering
    ↓              ↓              ↓              ↓
JSON Parser    GLSL Files   Topological    inja Engine
                            Sorting
```

## Key Features

### 1. Modular Shader Components

Each shader feature is a self-contained module:

```glsl
// lambert.glsl
vec3 GetDiffuseColor()
{
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(sky.sun_direction.xyz);
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 baseColor = GetAlbedo();
    vec3 lightColor = sky.sun_color.rgb * sky.sun_intensity;
    return baseColor * NdotL * lightColor;
}
```

### 2. Declarative Configuration

Material recipes describe shader composition:

```json
{
  "name": "Metal",
  "modules": {
    "lighting": "blinn_phong",
    "specular": "ggx",
    "ambient": "ibl_simple",
    "albedo": "texture_albedo"
  },
  "frag_color_body": "return GetDiffuseColor() + GetSpecularColor() + GetAmbientColor();"
}
```

### 3. Automatic Dependency Resolution

The engine automatically:
- Loads required modules
- Resolves dependencies between modules
- Orders modules correctly via topological sort
- Detects circular dependencies
- Reports missing dependencies

### 4. Template-Based Generation

Inja templates combine modules into complete shaders:

```glsl
// ========== Module Functions ==========
{{ module_functions }}

// ========== Fragment Color Composition ==========
vec3 ComputeFragColor()
{
    {{ frag_color_body }}
}
```

## Usage Example

```cpp
#include<hgl/shadergen/ShaderTemplateEngine.h>

using namespace hgl::graph;

// Initialize engine
ShaderTemplateEngine engine(
    AnsiString("ShaderLibrary/templates"),
    AnsiString("ShaderLibrary/modules")
);

// Load recipe
ShaderRecipe recipe = engine.LoadRecipe(
    AnsiString("ShaderLibrary/recipes/standard/metal.json"),
    AnsiString("high")  // quality level
);

// Generate shader
AnsiString shaderCode = engine.Generate(recipe);

// Use generated code...
```

## Testing

Run the test suite to validate:
```bash
./build/example/ShaderTemplateEngineTest ./
```

Tests cover:
1. Module loading and caching
2. Dependency resolution and topological sorting
3. Recipe loading from JSON
4. Complete shader code generation

## Integration with Existing Code

The system is designed to coexist with the existing ShaderGen system:

```cpp
class MaterialCreateInfo
{
    // Option 1: Use template engine
    bool CreateShaderFromTemplate(const AnsiString& recipePath,
                                  const AnsiString& qualityLevel);
    
    // Option 2: Use existing string concatenation
    bool CreateShader();  // existing method
};
```

This allows for:
- **Phase 1**: Parallel operation - both systems available
- **Phase 2**: Gradual migration - new materials use templates
- **Phase 3**: Complete switch - remove old system

## Benefits

### For Developers
- **Easier to Read**: GLSL code in separate files, not C++ strings
- **Faster Iteration**: Edit GLSL directly, no recompilation
- **Better Organization**: Clear module boundaries
- **Reusable Components**: Mix and match lighting models

### For Artists/Users
- **Pre-configured Recipes**: Ready-to-use materials
- **Quality Scaling**: High/medium/low quality presets
- **No Programming Required**: JSON configuration only

### For Maintainers
- **Reduced Complexity**: Less C++ string manipulation
- **Better Testing**: Individual modules testable
- **Clear Dependencies**: Explicit dependency declarations
- **Version Control Friendly**: GLSL and JSON diff nicely

## Extension Guide

### Adding a New Lighting Model

1. Create `ShaderLibrary/modules/lighting/toon.glsl`
2. Update `ShaderLibrary/modules/lighting/lighting_interface.json`
3. Use in recipes: `"lighting": "toon"`

### Creating a Custom Material

1. Copy an existing recipe
2. Modify module selections
3. Adjust color composition logic
4. Update descriptor bindings if needed

See `doc/ShaderTemplateEngine.md` for detailed guides.

## Performance Considerations

- **Module Caching**: Loaded modules stay in memory
- **Interface Caching**: JSON parsed once per category
- **Template Compilation**: inja caches compiled templates
- **Zero Runtime Overhead**: Generated code is pure GLSL

## Future Enhancements

Potential improvements:
- Compute shader support
- Geometry/tessellation shader templates
- Visual shader editor
- Hot-reload support for live editing
- Shader variant generation
- Performance profiling integration

## Statistics

- **Total Files**: 38 new files
- **Total Lines**: 2,480+ lines of code
- **Shader Modules**: 18 GLSL modules across 6 categories
- **Material Recipes**: 4 pre-configured materials
- **Documentation**: 539 lines
- **Test Code**: 258 lines

## Backward Compatibility

✅ **Fully Compatible**: The new system does not modify any existing code
✅ **Optional**: Can be adopted gradually
✅ **Coexists**: Works alongside current ShaderGen
✅ **No Breaking Changes**: Existing materials continue to work

## Conclusion

The Shader Template Engine provides a modern, maintainable approach to shader generation while maintaining full compatibility with existing code. It improves developer productivity, enables rapid iteration, and provides a foundation for future shader system enhancements.
