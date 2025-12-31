# Shader Material Recipes

This directory contains predefined material recipes that combine shader modules to create complete materials.

## Directory Structure

- **standard/** - Standard hard-surface materials
  - `metal.json` - Metallic materials with strong reflections
  - `wood.json` - Natural wood with diffuse lighting
  - `plastic.json` - Plastic with strong specular highlights

- **organic/** - Organic and soft materials
  - `skin.json` - Skin material with subsurface scattering approximation

## Recipe Format

Each recipe is a JSON file with the following structure:

```json
{
  "name": "MaterialName",
  "material_type": "Standard|Organic|Special",
  "rendering_path": "Forward|Deferred",
  "description": "Material description",
  
  "quality_levels": {
    "high": { /* High quality settings */ },
    "medium": { /* Medium quality settings */ },
    "low": { /* Low quality settings */ }
  }
}
```

### Quality Level Configuration

Each quality level can specify:

- **vertex_shader** - Vertex shader configuration
  - `template` - Template file to use
  - `vertex_inputs` - Input vertex attributes
  - `vertex_outputs` - Output variables

- **fragment_shader** - Fragment shader configuration
  - `template` - Template file to use
  - `inputs` - Input variables from vertex shader
  - `outputs` - Output render targets
  - `modules` - Map of module types to specific implementations
    - `lighting` - Lighting model (lambert, blinn_phong, pbr_standard, etc.)
    - `specular` - Specular model (phong, blinn_phong, ggx)
    - `ambient` - Ambient lighting (skylight, ibl_simple, ibl, sh)
    - `albedo` - Base color source (texture_albedo, vertex_color, constant_color)
    - `normal` - Normal calculation (vertex_normal, normal_map)
  - `frag_color_body` - GLSL code for combining lighting components
  - `alpha_expression` - Alpha channel calculation

- **descriptors** - Vulkan descriptor bindings
  - `ubos` - Uniform buffer objects
  - `samplers` - Texture samplers

## Usage Example

To load and use a recipe in code:

```cpp
ShaderTemplateEngine engine(
    "/path/to/ShaderLibrary/templates",
    "/path/to/ShaderLibrary/modules"
);

// Load metal material at high quality
ShaderRecipe recipe = engine.LoadRecipe(
    "/path/to/ShaderLibrary/recipes/standard/metal.json",
    "high"
);

// Generate shader code
AnsiString shaderCode = engine.Generate(recipe);
```

## Creating Custom Recipes

1. Copy an existing recipe as a starting point
2. Modify the `modules` section to use different shader modules
3. Adjust `frag_color_body` to combine lighting components as desired
4. Update `descriptors` to match the requirements of your modules
5. Test with different quality levels for performance scaling

## Available Modules

### Lighting Models
- `lambert` - Simple Lambertian diffuse
- `half_lambert` - Half-Lambert for better shadow contrast
- `blinn_phong` - Classic Blinn-Phong lighting
- `pbr_standard` - Full PBR with energy conservation

### Specular Models
- `phong` - Classic Phong specular
- `blinn_phong` - Blinn-Phong specular (faster)
- `ggx` - PBR GGX/Trowbridge-Reitz specular

### Ambient Lighting
- `skylight` - Simple sky color ambient
- `ibl_simple` - Basic image-based lighting
- `ibl` - Full IBL with diffuse and specular convolution
- `sh` - Spherical harmonics ambient

### Albedo Sources
- `texture_albedo` - Base color from texture
- `vertex_color` - Base color from vertex colors
- `constant_color` - Fixed color from material instance

### Normal Calculation
- `vertex_normal` - Use vertex normal directly
- `normal_map` - Calculate from normal map texture
