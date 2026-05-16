#include <hgl/shadergen/MITSSBOEmitter.h>
#include <hgl/shadergen/MaterialDescriptorDB.h>
#include <hgl/shadergen/ShaderWriter.h>
#include <hgl/shadergen/InterstageVaryingLayout.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/common/ShaderDescriptorDef.h>
#include <hgl/log/Log.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace hgl::graph
{
namespace
{
    static bool IsArraySampler2D(const char *type_name)
    {
        return type_name && std::strcmp(type_name, "sampler2DArray") == 0;
    }
}

std::string EmitMaterialInstanceTextureGLSL(const MaterialDescriptorDB &mdi, ShaderStage stage)
{
    const uint32_t stage_bit = uint32_t(stage);

    // Collect array sampler slots.
    std::vector<mtl::SamplerSlot> array_slots;

    auto try_collect = [&](const auto *sd)
    {
        if (!sd || sd->set < 0 || sd->binding < 0) return;
        if (!IsArraySampler2D(sd->type.c_str())) return;

        mtl::SamplerSlot slot = mtl::SamplerSlot::BaseColor;
        if (mtl::TryGetSlotFromDescriptorName(sd->name, slot))
            array_slots.push_back(slot);
    };

    for (size_t i = 0; i < mtl::SamplerSlotCount; ++i)
    {
        if (const TextureDescriptor *tex = mdi.GetTexture(mtl::SamplerSlot(i)))
            if (tex->stage_flag & stage_bit)
                try_collect(tex);

        if (const TextureSamplerDescriptor *samp = mdi.GetTextureSampler(mtl::SamplerSlot(i)))
            if (samp->stage_flag & stage_bit)
                try_collect(samp);
    }

    GLogInfo("[MITSSBOEmitter] EmitMaterialInstanceTextureGLSL: stage=%u array_slots=%zu",
             uint32_t(stage), array_slots.size());
    for (const mtl::SamplerSlot s : array_slots)
        GLogInfo("[MITSSBOEmitter]   MIT slot=%s", mtl::SamplerSlotNameList[uint8(s)]);

    if (array_slots.empty())
        return {};

    // Sort and deduplicate
    std::sort(array_slots.begin(), array_slots.end());
    array_slots.erase(std::unique(array_slots.begin(), array_slots.end()), array_slots.end());

    std::string out;
    ShaderWriter writer(out);
    out.reserve(512);
    writer.EmitLine("// ---- Auto-generated MaterialInstanceTexture SSBO ----");

    // Emit the FS input for MaterialInstanceID via the canonical varying layout.
    {
        const std::string mi_decl = EmitFSInput(InterstageVarying::MaterialInstanceID);
        writer.EmitLine("#ifndef ULRE_HAS_FRAG_MATERIAL_INSTANCE_ID");
        out += mi_decl;
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

    // 1. Emit compact per-instance stride and per-slot index defines.
    writer.EmitDefine("MIT_TEXTURE_COUNT", std::to_string(array_slots.size()).c_str());
    for (size_t i = 0; i < array_slots.size(); ++i)
    {
        const mtl::SamplerSlot slot = array_slots[i];
        const std::string upper_name = mtl::ToUpperASCII(mtl::SamplerSlotNameList[uint8(slot)]);
        writer.EmitDefine("MIT_" + upper_name + "_IDX", std::to_string(i).c_str());
    }
    writer.NewLine();

    // 2. SSBO layout (uses PERMATERIAL_SET / MBI_TEXTURE_BINDING from layout defines).
    writer.EmitLine("layout(std430, set=PERMATERIAL_SET, binding=MBI_TEXTURE_BINDING) readonly buffer MaterialBindingInstanceTexture").BeginBlock();
    writer.EmitLine("uint tex_id[];");
    writer.EndBlock(hgl::graph::ShaderWriter::EndBlockMode::NamedInstance, "mit").NewLine();

    // 3. Per-slot inline layer-index accessors.
    for (const mtl::SamplerSlot slot : array_slots)
    {
        const char *name = mtl::SamplerSlotNameList[uint8(slot)];
        const std::string upper_name = mtl::ToUpperASCII(name);
        std::string fn;
        fn += "uint GetMITLayer_"; fn += name;
        fn += "(uint mi_id) { return mit.tex_id[mi_id * MIT_TEXTURE_COUNT + MIT_";
        fn += upper_name; fn += "_IDX]; }";
        writer.EmitLine(fn);
    }

    writer.EmitLine("// ------------------------------------------------------").NewLine();
    return out;
}
} // namespace hgl::graph
