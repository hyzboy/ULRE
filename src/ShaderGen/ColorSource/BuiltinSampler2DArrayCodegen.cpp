#include "BuiltinSamplerCodegen.h"
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/shadergen/ShaderWriter.h>
#include <hgl/shadergen/InterstageVaryingLayout.h>
#include <hgl/log/Log.h>
#include <algorithm>
#include <string>

namespace hgl::graph
{

// ── BuiltinSampler2DArrayCodegen ─────────────────────────────────────────────

void BuiltinSampler2DArrayCodegen::EmitDeclarations(ShaderWriter          &writer,
                                                    const ColorSource      &src,
                                                    const ResolvedBindings &resolved_bindings) const
{
    const std::string &debug_name = src.bindings[0].debug_name;
    uint32_t set = 0, binding = 0;
    FindResolved(resolved_bindings, debug_name, set, binding);

    const char *sampler_symbol = mtl::ToGLSLSamplerSymbol(src.slot);

    writer.EmitLayoutBinding(set, binding)
          .EmitUniform("sampler2DArray", sampler_symbol);
}

void BuiltinSampler2DArrayCodegen::EmitGroupAuxiliary(
    ShaderWriter                      &writer,
    const std::vector<ColorSource>    &group,
    const ResolvedBindings            & /*resolved_bindings*/) const
{
    // Collect sorted, deduplicated array slots from the group.
    std::vector<mtl::SamplerSlot> array_slots;
    array_slots.reserve(group.size());
    for (const auto &cs : group)
        array_slots.push_back(cs.slot);
    std::sort(array_slots.begin(), array_slots.end());
    array_slots.erase(std::unique(array_slots.begin(), array_slots.end()), array_slots.end());

    GLogInfo("[BuiltinSampler2DArrayCodegen] EmitGroupAuxiliary: array_slots=%zu", array_slots.size());
    for (const mtl::SamplerSlot s : array_slots)
        GLogInfo("[BuiltinSampler2DArrayCodegen]   slot=%s", mtl::SamplerSlotNameList[uint8_t(s)]);

    // Emit FS input for MaterialInstanceID.
    {
        const std::string mi_decl = EmitFSInput(InterstageVarying::MaterialInstanceID);
        writer.EmitLine("#ifndef ULRE_HAS_FRAG_MATERIAL_INSTANCE_ID");
        writer.EmitLine(mi_decl);
        writer.EmitLine("#define ULRE_HAS_FRAG_MATERIAL_INSTANCE_ID");
        writer.EmitLine("#endif");
    }
    writer.EmitLine("#ifndef MATERIAL_INSTANCE_ID_OVERRIDE");
    writer.EmitLine("#define MATERIAL_INSTANCE_ID_OVERRIDE fragMaterialInstanceID");
    writer.EmitLine("#endif");
    writer.EmitLine("#ifndef ULRE_HAS_GET_MATERIAL_INSTANCE_ID");
    writer.EmitLine("#define ULRE_HAS_GET_MATERIAL_INSTANCE_ID");
    writer.EmitLine("uint GetMaterialInstanceID() { return MATERIAL_INSTANCE_ID_OVERRIDE; }");
    writer.EmitLine("#endif");

    // Per-instance stride and per-slot index defines.
    writer.EmitDefine("MIT_TEXTURE_COUNT", std::to_string(array_slots.size()).c_str());
    for (size_t i = 0; i < array_slots.size(); ++i)
    {
        const std::string upper_name = mtl::ToUpperASCII(mtl::SamplerSlotNameList[uint8_t(array_slots[i])]);
        writer.EmitDefine("MIT_" + upper_name + "_IDX", std::to_string(i).c_str());
    }
    writer.NewLine();

    // SSBO layout.
    writer.EmitLine("layout(std430, set=PERMATERIAL_SET, binding=MBI_TEXTURE_BINDING) readonly buffer MaterialBindingInstanceTexture").BeginBlock();
    writer.EmitLine("uint tex_id[];");
    writer.EndBlock(ShaderWriter::EndBlockMode::NamedInstance, "mit").NewLine();

    // Per-slot layer-index accessors.
    for (const mtl::SamplerSlot slot : array_slots)
    {
        const char *name = mtl::SamplerSlotNameList[uint8_t(slot)];
        const std::string upper_name = mtl::ToUpperASCII(name);
        std::string fn = "uint GetMITLayer_"; fn += name;
        fn += "(uint mi_id) { return mit.tex_id[mi_id * MIT_TEXTURE_COUNT + MIT_";
        fn += upper_name; fn += "_IDX]; }";
        writer.EmitLine(fn);
    }

    writer.EmitLine("// ------------------------------------------------------").NewLine();
}

void BuiltinSampler2DArrayCodegen::EmitGetterFunction(ShaderWriter          &writer,
                                                      const ColorSource      &src,
                                                      const ResolvedBindings & /*resolved_bindings*/) const
{
    const char *sampler_symbol = mtl::ToGLSLSamplerSymbol(src.slot);
    const char *slot_name      = mtl::SamplerSlotNameList[uint8_t(src.slot)];

    std::string tmp;
    if (src.builtin.output_format == ColorSourceOutputFormat::Grayscale_R)
    {
        tmp += "vec4 GetSampler"; tmp += slot_name;
        tmp += "(uint mi_id, vec2 uv) { uint _layer = GetMITLayer_"; tmp += slot_name;
        tmp += "(mi_id);"
               " float r = texture("; tmp += sampler_symbol;
        tmp += ", vec3(uv, float(_layer))).r; return vec4(r,r,r,r); }\n";
    }
    else
    {
        tmp += "vec4 GetSampler"; tmp += slot_name;
        tmp += "(uint mi_id, vec2 uv) { uint _layer = GetMITLayer_"; tmp += slot_name;
        tmp += "(mi_id);"
               " return texture("; tmp += sampler_symbol;
        tmp += ", vec3(uv, float(_layer))); }\n";
    }
    writer.EmitLine(tmp);
}

} // namespace hgl::graph
