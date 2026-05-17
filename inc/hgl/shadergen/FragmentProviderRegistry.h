#pragma once

// inc/hgl/shadergen/FragmentProviderRegistry.h
//
// FragmentProvider registry: maps a FragmentProviderId to its GLSL include
// path implementing the fragment data source contract.
//
// ─── PROVIDER CONTRACT (replaces_input_resolve == true) ──────────────────────
//
//   SurfaceInput GetSurfaceInput()
//     — the provider directly constructs and returns a SurfaceInput.
//       CompositorAssembler sets PCG_FRAGMENT_PROVIDER and skips the standard
//       frag_input_resolve.glsl / ResolveSurfaceInput() call.
//
// ─── PROVIDER CONTRACT (replaces_input_resolve == false) ─────────────────────
//
//   void AppendSurfaceInput(inout SurfaceInput si)
//     — the provider patches specific fields of an already-resolved SurfaceInput.
//       frag_input_resolve.glsl is still emitted first.
//
// ─── ADDING A NEW PROVIDER ───────────────────────────────────────────────────
//
// 1. Add a value to FragmentProviderId below.
// 2. Create ShaderLibrary/fragment_provider/<name>.glsl.
// 3. Add a kBuiltinFragmentProviders entry in FragmentProviderRegistry.cpp.
// 4. Update CompositorAssembler::BuildForwardFragmentEntry() if needed.
//
// ─────────────────────────────────────────────────────────────────────────────

#include <string_view>
#include <cstdint>

namespace hgl::graph
{

enum class FragmentProviderId : uint8_t
{
    Default      = 0,  ///< Standard: frag_input_resolve.glsl populates SurfaceInput from varyings.
    PCG_FragCoord,     ///< PCG: derive SurfaceInput fields from gl_FragCoord (no varyings).
    COUNT,
};

struct FragmentProviderDescriptor
{
    FragmentProviderId  id;
    std::string_view    glsl_path;              ///< relative to ShaderLibrary root; empty = Default path
    bool                needs_camera;
    bool                needs_viewport;
    bool                replaces_input_resolve; ///< true => implements GetSurfaceInput()
};

/// Returns a pointer to the built-in descriptor for @p id, or nullptr if unknown.
const FragmentProviderDescriptor *FindBuiltinFragmentProvider(FragmentProviderId id) noexcept;

} // namespace hgl::graph
