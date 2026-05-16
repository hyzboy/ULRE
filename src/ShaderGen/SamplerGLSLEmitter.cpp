#include <hgl/shadergen/SamplerGLSLEmitter.h>
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
    static bool CStrEq(const char *lhs, const char *rhs)
    {
        return lhs && rhs && std::strcmp(lhs, rhs) == 0;
    }

    static bool IsSimpleSampler2D(const char *type_name)
    {
        return CStrEq(type_name, "sampler2D");
    }

    static bool IsArraySampler2D(const char *type_name)
    {
        return CStrEq(type_name, "sampler2DArray");
    }

    static void AppendKnownSlotSampler(std::string &out, const ShaderDescriptor *descriptor, const mtl::SamplerSlot slot, const TextureChannelHint channel_hint)
    {
        const char *sampler_symbol = mtl::ToGLSLSamplerSymbol(slot);
        const char *slot_name      = mtl::SamplerSlotNameList[uint8(slot)];
        ShaderWriter writer(out);

        writer.EmitLayoutBinding(descriptor->set, descriptor->binding)
              .EmitUniform("sampler2D", sampler_symbol);

        // Ensure GetMaterialInstanceID() is always available for the unified 2-arg call form.
        // (MIT SSBO path defines it via ssbo_material_instance.glsl; simple sampler path may not.)
        out += "#ifndef ULRE_HAS_GET_MATERIAL_INSTANCE_ID\n";
        out += "#define ULRE_HAS_GET_MATERIAL_INSTANCE_ID\n";
        out += "uint GetMaterialInstanceID() { return 0u; }\n";
        out += "#endif\n";

        // Inline getter — unified 2-arg signature; mi_id ignored for plain sampler2D.
        if (channel_hint == TextureChannelHint::Grayscale)
        {
            out += "vec4 GetSampler"; out += slot_name;
            out += "(uint /*mi_id*/, vec2 uv) { float r = texture("; out += sampler_symbol;
            out += ", uv).r; return vec4(r,r,r,r); }\n";
        }
        else
        {
            out += "vec4 GetSampler"; out += slot_name;
            out += "(uint /*mi_id*/, vec2 uv) { return texture("; out += sampler_symbol;
            out += ", uv); }\n";
        }

        writer.NewLine();
    }

    static void AppendKnownSlotArraySampler(std::string &out, const ShaderDescriptor *descriptor, const mtl::SamplerSlot slot, const TextureChannelHint channel_hint)
    {
        const char *sampler_symbol = mtl::ToGLSLSamplerSymbol(slot);
        const char *slot_name      = mtl::SamplerSlotNameList[uint8(slot)];
        GLogInfo("[SamplerGLSLEmitter] AppendKnownSlotArraySampler: slot=%s symbol=%s set=%d binding=%d stage_flag=0x%08X channel_hint=%d",
                 slot_name, sampler_symbol, descriptor->set, descriptor->binding,
                 descriptor->stage_flag, int(channel_hint));
        ShaderWriter writer(out);

        writer.EmitLayoutBinding(descriptor->set, descriptor->binding)
              .EmitUniform("sampler2DArray", sampler_symbol);

        // Inline getter — layer index received as explicit mi_id parameter (Step 5).
        if (channel_hint == TextureChannelHint::Grayscale)
        {
            out += "vec4 GetSampler"; out += slot_name;
            out += "(uint mi_id, vec2 uv) { uint _layer = GetMITLayer_"; out += slot_name;
            out += "(mi_id);";
            out += " float r = texture("; out += sampler_symbol;
            out += ", vec3(uv, float(_layer))).r; return vec4(r,r,r,r); }\n";
        }
        else
        {
            out += "vec4 GetSampler"; out += slot_name;
            out += "(uint mi_id, vec2 uv) { uint _layer = GetMITLayer_"; out += slot_name;
            out += "(mi_id);";
            out += " return texture("; out += sampler_symbol;
            out += ", vec3(uv, float(_layer))); }\n";
        }

        writer.NewLine();
    }

    static void AppendGenericSampler(std::string &out, const ShaderDescriptor *descriptor)
    {
        ShaderWriter writer(out);
        writer.EmitLayoutBinding(descriptor->set, descriptor->binding)
              .EmitUniform("sampler2D", descriptor->name)
              .NewLine();
    }
}

std::string EmitSimpleSamplerGLSL(const MaterialDescriptorDB &mdi, ShaderStage stage)
{
    const uint32_t stage_bit = uint32_t(stage);

    struct SamplerEntry
    {
        const ShaderDescriptor *descriptor;
        TextureChannelHint channel_hint;
    };

    std::vector<SamplerEntry> samplers;
    std::vector<SamplerEntry> array_samplers;

    auto collect = [&](const auto *sd)
    {
        if (!sd || sd->set < 0 || sd->binding < 0) return;
        if (IsSimpleSampler2D(sd->type.c_str()))
            samplers.push_back({sd, sd->channel_hint});
        else if (IsArraySampler2D(sd->type.c_str()))
            array_samplers.push_back({sd, sd->channel_hint});
    };

    for (size_t i = 0; i < mtl::SamplerSlotCount; ++i)
    {
        if (const TextureDescriptor *tex = mdi.GetTexture(mtl::SamplerSlot(i)))
            if (tex->stage_flag & stage_bit)
                collect(tex);

        if (const TextureSamplerDescriptor *samp = mdi.GetTextureSampler(mtl::SamplerSlot(i)))
            if (samp->stage_flag & stage_bit)
                collect(samp);
    }

    GLogInfo("[SamplerGLSLEmitter] EmitSimpleSamplerGLSL: stage=%u simple=%zu array=%zu",
             uint32_t(stage), samplers.size(), array_samplers.size());
    for (const SamplerEntry &e : samplers)
        GLogInfo("[SamplerGLSLEmitter]   simple  name=%s desc_type=%d set=%d binding=%d stage_flag=0x%08X",
                 e.descriptor->name, int(e.descriptor->desc_type), e.descriptor->set, e.descriptor->binding, e.descriptor->stage_flag);
    for (const SamplerEntry &e : array_samplers)
        GLogInfo("[SamplerGLSLEmitter]   array   name=%s desc_type=%d set=%d binding=%d stage_flag=0x%08X",
                 e.descriptor->name, int(e.descriptor->desc_type), e.descriptor->set, e.descriptor->binding, e.descriptor->stage_flag);

    if (samplers.empty() && array_samplers.empty())
        return {};

    auto sort_by_binding = [](const SamplerEntry &lhs, const SamplerEntry &rhs)
    {
        if (lhs.descriptor->set != rhs.descriptor->set)
            return lhs.descriptor->set < rhs.descriptor->set;
        if (lhs.descriptor->binding != rhs.descriptor->binding)
            return lhs.descriptor->binding < rhs.descriptor->binding;
        return std::strcmp(lhs.descriptor->name, rhs.descriptor->name) < 0;
    };
    std::sort(samplers.begin(),       samplers.end(),       sort_by_binding);
    std::sort(array_samplers.begin(), array_samplers.end(), sort_by_binding);

    std::string out;
    out.reserve(512);
    out += "// ---- Auto-generated simple sampler declarations ----\n";

    for (const SamplerEntry &entry : samplers)
    {
        mtl::SamplerSlot slot = mtl::SamplerSlot::BaseColor;
        if (mtl::TryGetSlotFromDescriptorName(entry.descriptor->name, slot))
            AppendKnownSlotSampler(out, entry.descriptor, slot, entry.channel_hint);
        else
            AppendGenericSampler(out, entry.descriptor);
    }

    for (const SamplerEntry &entry : array_samplers)
    {
        mtl::SamplerSlot slot = mtl::SamplerSlot::BaseColor;
        mtl::TryGetSlotFromDescriptorName(entry.descriptor->name, slot);

        // Array sampler getters call GetMITLayer_* / MATERIAL_INSTANCE_ID_OVERRIDE which are
        // only available in FS (injected via ssbo_material_instance.glsl).
        // For VS (and other non-FS stages) emit only the uniform declaration — no getter body.
        if (stage == ShaderStage::Fragment)
        {
            AppendKnownSlotArraySampler(out, entry.descriptor, slot, entry.channel_hint);
        }
        else
        {
            ShaderWriter writer(out);
            writer.EmitLayoutBinding(entry.descriptor->set, entry.descriptor->binding)
                  .EmitUniform("sampler2DArray", mtl::ToGLSLSamplerSymbol(slot))
                  .NewLine();
        }
    }

    out += "// ----------------------------------------------------\n\n";
    return out;
}

std::string EmitMaterialInstanceTextureGLSL(const MaterialDescriptorDB &mdi, ShaderStage stage)
{
    const uint32_t stage_bit = uint32_t(stage);

    // Collect array sampler slots (same detection as EmitSimpleSamplerGLSL).
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

    GLogInfo("[SamplerGLSLEmitter] EmitMaterialInstanceTextureGLSL: stage=%u array_slots=%zu",
             uint32_t(stage), array_slots.size());
    for (const mtl::SamplerSlot s : array_slots)
        GLogInfo("[SamplerGLSLEmitter]   MIT slot=%s", mtl::SamplerSlotNameList[uint8(s)]);

    if (array_slots.empty())
        return {};

    // Sort and deduplicate
    std::sort(array_slots.begin(), array_slots.end());
    array_slots.erase(std::unique(array_slots.begin(), array_slots.end()), array_slots.end());

    std::string out;
    ShaderWriter writer(out);
    out.reserve(512);
    writer.EmitLine("// ---- Auto-generated MaterialInstanceTexture SSBO ----");

    // Emit the FS input for MaterialInstanceID via the canonical varying layout
    // (avoids hardcoding location=0 here).
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

    // 3. Per-slot inline layer-index accessors — no global variables needed.
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
}
