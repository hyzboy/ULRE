#pragma once

#include <hgl/mtl/MaterialDef.h>
#include <hgl/mtl/new/ShaderPermutationKey.h>
#include <string>

namespace hgl::graph
{
    /**
     * InjaAssembler — renders inja GLSL templates from a MaterialDef.
     *
     * Replaces the C++ flag-to-#define routing in CompositorAssembler::
     * BuildForwardVertexEntry() / BuildForwardFragmentEntry() with inja
     * template files (.inja) stored alongside the existing .glsl files.
     *
     * Workflow:
     *   1. Call CanAssemble(def) — returns true when def.vs_template and
     *      def.fs_template are both non-empty.
     *   2. Call Assemble(def, spk) — reads the .inja files from
     *      shader_library_path_, derives feature-flag variables from the
     *      MaterialDef, renders the templates, injects SPK #defines, and
     *      returns the complete GLSL strings.
     *   3. On failure (template missing, render error), success=false and
     *      error is populated; caller falls back to CompositorAssembler.
     */
    class InjaAssembler
    {
    public:

        struct AssembleResult
        {
            std::string vertex_glsl;
            std::string fragment_glsl;
            bool        success = false;
            std::string error;
        };

        /// Uses the global ShaderLibrary path from ShaderGenPathConfig.
        InjaAssembler();

        /// shader_library_path: absolute path to the ShaderLibrary root (no trailing slash).
        explicit InjaAssembler(const std::string &shader_library_path);

        /// Returns true when def.vs_template and def.fs_template are both non-empty.
        bool CanAssemble(const mtl::MaterialDef &def) const;

        /// Renders the inja templates referenced by def, deriving feature-flag
        /// variables from the MaterialDef fields (vertex attribs, UBO/SSBO
        /// descriptors, blend mode, surface type) and from def.bool_features
        /// overrides.  SPK #defines are injected after the first #version line.
        AssembleResult Assemble(
            const mtl::MaterialDef          &def,
            const ShaderPermutationKey      &spk = {}
        ) const;

    private:
        std::string shader_lib_path_;
    };

} // namespace hgl::graph
