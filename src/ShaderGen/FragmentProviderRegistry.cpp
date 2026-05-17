#include <hgl/shadergen/FragmentProviderRegistry.h>

namespace hgl::graph
{

static constexpr FragmentProviderDescriptor kBuiltinFragmentProviders[] =
{
    {
        FragmentProviderId::Default,
        /*glsl_path=*/ "",          // CompositorAssembler falls back to frag_input_resolve.glsl
        /*needs_camera=*/   false,
        /*needs_viewport=*/ false,
        /*replaces_input_resolve=*/ false,
    },
    {
        FragmentProviderId::PCG_FragCoord,
        /*glsl_path=*/ "fragment_provider/pcg_fragcoord.glsl",
        /*needs_camera=*/   false,
        /*needs_viewport=*/ true,   // uses viewport.canvas_resolution for normalisation
        /*replaces_input_resolve=*/ true,
    },
};

const FragmentProviderDescriptor *FindBuiltinFragmentProvider(FragmentProviderId id) noexcept
{
    for (const auto &d : kBuiltinFragmentProviders)
        if (d.id == id)
            return &d;
    return nullptr;
}

} // namespace hgl::graph
