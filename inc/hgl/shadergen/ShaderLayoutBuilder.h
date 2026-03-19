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
    ///      VertexAttrib::TransformID → "TRANSFORM_ID_LOCATION"
    /// Returns nullptr for unknown attribs.
    const char *GetVertexAttribLocationMacroName(VertexAttrib attrib);

    /// Returns the layout-macro name for a descriptor set type.
    /// e.g. DescriptorSetType::Scene     → "SCENE_SET"
    ///      DescriptorSetType::Transform → "TRANSFORM_SET"
    /// Returns nullptr for Unknow / out-of-range.
    const char *GetDescriptorSetMacroName(DescriptorSetType set_type);

    /// Returns the layout-macro name for a descriptor binding by its GLSL name.
    /// Well-known names are mapped directly:
    ///   viewport → VIEWPORT_BINDING
    ///   camera   → CAMERA_BINDING
    ///   sky      → SKY_BINDING
    ///   l2w      → L2W_BINDING
    ///   tid      → TID_BINDING
    ///   mid      → MID_BINDING
    ///   mtl      → MI_BINDING
    /// All other names follow the rule: UPPER(name) + "_BINDING".
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
