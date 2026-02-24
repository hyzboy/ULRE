# Material Shader Generator — Refactoring Summary

## Background

The material shader generator (`inc/hgl/graph/mtl`, `inc/hgl/shadergen`, `src/ShaderGen`)
generates GLSL shaders at runtime for every built-in material (PureColor2D, Gizmo3D, BasicLit,
etc.). Materials are described as C++ objects that assemble vertex/fragment/geometry shader
code blocks via `MaterialCreateInfo`.

## Problems with the Original Design

### 1. Config headers triggered factory auto-registration

`Material2DCreateConfig.h` and `Material3DCreateConfig.h` used `DEFINE_MATERIAL_FACTORY_CLASS`
at the bottom of the file. This macro **both** declared the `Create*` functions **and** inserted
anonymous-namespace static objects that call `RegisterMaterialFactory()` at program startup.

**Consequence**: any translation unit (TU) that included a config header for its struct
definitions (e.g. to build a `Material3DCreateConfig` value) would also create and attempt to
register factory objects for every built-in material, even though the caller had no interest
in the factory system. Duplicate registrations were silently discarded, but the `new`/`delete`
overhead accumulated.

### 2. `Std2DMaterial` / `Std3DMaterial` were private implementation details

`src/ShaderGen/2d/Std2DMaterial.h` and `src/ShaderGen/3d/Std3DMaterial.h` were located under
`src/` and therefore not available to code outside the `ShaderGen` library. Any developer who
wanted to create a custom material by deriving from these classes had to modify the library
itself.

### 3. `SBS_Billboard` struct was malformed

`MFBillboard.h` initialized `ShaderBufferSource` with only 3 fields (missing `set_type`) and
the `GetBillboardPosition` function had a missing `return` statement.

---

## Changes Made

### `inc/hgl/graph/mtl/MaterialLibrary.h`

Added two focused macros:

| Macro | Purpose |
|-------|---------|
| `DECLARE_MATERIAL_CREATOR(name, cfg_type)` | Declares `Create<name>(dev_attr, cfg*)` and the `inline_material::name` string constant. **No factory registration.** Used in config headers. |
| `IMPL_MATERIAL_FACTORY(name, cfg_type)` | Defines the factory class and the static auto-registration object. **Requires `DECLARE_MATERIAL_CREATOR` to have been expanded first.** Used only in the dedicated factory headers. |

`DEFINE_MATERIAL_FACTORY_CLASS` is now a convenience alias for `DECLARE_MATERIAL_CREATOR` +
`IMPL_MATERIAL_FACTORY` for backwards compatibility.

### `inc/hgl/graph/mtl/Material2DCreateConfig.h` / `Material3DCreateConfig.h`

All `DEFINE_MATERIAL_FACTORY_CLASS` calls replaced with `DECLARE_MATERIAL_CREATOR`. The config
headers now only declare the `Create*` functions; they contain **no factory registration code**.

### New: `inc/hgl/graph/mtl/MaterialFactory2D.h` / `MaterialFactory3D.h`

Dedicated factory registration headers. Each includes the corresponding config header and calls
`IMPL_MATERIAL_FACTORY` for every built-in material. These headers should be included **only in
TUs that need the factory system** (primarily `MaterialManager.cpp`).

### New: `inc/hgl/graph/mtl/Std2DMaterial.h` / `Std3DMaterial.h`

The base classes for standard 2D / 3D materials are now in public include paths. External code
(outside the `ShaderGen` library) can derive from them to create custom materials without
modifying library internals.

`src/ShaderGen/2d/Std2DMaterial.h` and `src/ShaderGen/3d/Std3DMaterial.h` now simply
`#include` the public versions.

### `src/SceneGraph/module/MaterialManager.cpp`

Added `#include<hgl/graph/mtl/MaterialFactory2D.h>` and
`#include<hgl/graph/mtl/MaterialFactory3D.h>`. This single TU now owns all factory
registrations, and the references it introduces to the `Create*` functions force the linker
to pull in all built-in material implementation objects.

### `src/ShaderGen/common/MFBillboard.h`

Fixed `SBS_Billboard`: added the missing `DescriptorSetType::PerMaterial` field and corrected
field order. Fixed the missing `return` statement in `GetBillboardPosition`.

---

## New Include Convention

| Goal | What to include |
|------|----------------|
| Use config struct only (e.g. build a `Material3DCreateConfig`) | `Material3DCreateConfig.h` |
| Call a `Create*` function directly | `Material3DCreateConfig.h` (unchanged) |
| Use `inline_material::` name constants | `Material3DCreateConfig.h` (unchanged) |
| Register ALL built-in factories (MaterialManager) | `MaterialFactory2D.h` + `MaterialFactory3D.h` |
| Create a custom material derived from the standard base | `Std2DMaterial.h` or `Std3DMaterial.h` |
