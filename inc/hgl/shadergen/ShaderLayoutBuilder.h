#pragma once

#include <hgl/shadergen/ShaderLayoutContract.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/DescriptorSetTypeDef.h>
#include <string>

namespace hgl::graph
{
    namespace mtl { class MaterialCreateInfo; }

    // ────────────────────────────────────────────────────────────
    // Naming helpers
    // ────────────────────────────────────────────────────────────

    /// Returns the layout-macro name for a vertex attribute location.
    /// e.g. VertexAttrib::Position  → "POSITION_LOCATION"
    /// Macro names are derived from GetVertexAttribName(VertexAttrib).
    /// Returns an empty string for unknown attribs.
    std::string GetVertexAttribLocationMacroName(VertexAttrib attrib);

    /// Returns the layout-macro name for a descriptor set type.
    /// e.g. DescriptorSetType::Static      → "STATIC_SET"
    ///      DescriptorSetType::PerObject   → "PEROBJECT_SET"
    ///      DescriptorSetType::PerMaterial → "PERMATERIAL_SET"
    /// Returns nullptr for Unknow / out-of-range.
    const char *GetDescriptorSetMacroName(DescriptorSetType set_type);

    /// Returns the layout-macro name for a descriptor binding by its GLSL name.
    /// Semantic descriptors use DescriptorBindingContract metadata, texture slots use
    /// SamplerName helpers, and all others follow UPPER(name) + "_BINDING".
    std::string GetDescriptorBindingMacroName(const char *descriptor_name);

    // ────────────────────────────────────────────────────────────
    // Builder
    // ────────────────────────────────────────────────────────────

    /**
     * Builds a ShaderLayoutContract from a fully-populated MaterialCreateInfo.
     *
     * Prerequisites:
     *   - All descriptor entries must have been added (AddUBO / AddSSBO / etc.)
     *   - All vertex inputs must have been added (vsc->AddInput)
     *   - Resort() must have been called so ShaderDescriptor::set / ::binding
     *     hold their final allocated numbers.
     *   - The caller is responsible for calling mci.Resort() before this.
     *
     * Returns a contract with entries sorted ascending by value within each bucket.
     */
    ShaderLayoutContract BuildShaderLayoutContract(const mtl::MaterialCreateInfo &mci);

}  // namespace hgl::graph
