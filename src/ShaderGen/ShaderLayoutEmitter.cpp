/// ShaderLayoutEmitter.cpp
///
/// Converts a ShaderLayoutContract into a GLSL #define block that can be
/// prepended to any shader stage source.

#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/shadergen/ShaderWriter.h>
#include <hgl/shadergen/AttributeProviderRegistry.h>
#include <hgl/common/DescriptorSetTypeDef.h>

#include <ostream>
#include <string>

namespace hgl::graph
{

static void AppendSection(std::string       &out,
                          const char        *section_comment,
                          const std::vector<ShaderLayoutEntry> &entries)
{
    if (entries.empty())
        return;

    ShaderWriter writer(out);

    writer.EmitLine(std::string("// ") + section_comment);

    for (const ShaderLayoutEntry &e : entries)
    {
        const std::string value_text = std::to_string(e.value);
        writer.EmitDefine(e.macro_name, value_text.c_str());
    }
}

std::string EmitShaderLayoutDefines(const ShaderLayoutContract &contract)
{
    if (contract.Empty())
        return {};

    std::string out;
    ShaderWriter writer(out);
    out.reserve(512);

    writer.EmitCommentLine("EmitShaderLayoutDefines.Begin");

    AppendSection(out, "Vertex input locations", contract.vertex_locations);
    AppendSection(out, "Descriptor sets",        contract.descriptor_sets);
    AppendSection(out, "Descriptor bindings",    contract.descriptor_bindings);

    writer.EmitCommentLine("EmitShaderLayoutDefines.End");
    return out;
}

std::string DumpShaderLayoutContract(const ShaderLayoutContract &contract)
{
    if (contract.Empty())
        return "(empty ShaderLayoutContract)\n";

    std::string out;
    out.reserve(256);

    auto dump_section = [&](const char *title, const std::vector<ShaderLayoutEntry> &entries)
    {
        if (entries.empty())
            return;
        out += "  [";
        out += title;
        out += "]\n";
        for (const ShaderLayoutEntry &e : entries)
        {
            out += "    ";
            out += e.macro_name;
            out += " = ";
            out += std::to_string(e.value);
            out += '\n';
        }
    };

    dump_section("Vertex input locations", contract.vertex_locations);
    dump_section("Descriptor sets",        contract.descriptor_sets);
    dump_section("Descriptor bindings",    contract.descriptor_bindings);

    return out;
}

void EmitPositionInput(std::ostream &out,
                       const PositionProvider &p,
                       int position_location)
{
    if (p.id == PositionProviderId::DirectVec3)
    {
        out << "layout(location=" << position_location << ") in vec3 inPosition;\n";
        out << "#define GetPositionLocal() (inPosition)\n";
    }
    else
    {
        if (p.vab_count > 0)
            out << "#define POSITION_LOCATION " << position_location << "\n";
        out << "#include \"" << p.glsl_path << "\"\n";
    }
}

void EmitAttribInput(std::ostream &out, const mtl::MaterialVariantKey &key)
{
    constexpr int kVertexStreamsSet = int(SET_TYPE_VERTEX_STREAMS);

    for (uint32_t i = 0; i < uint32_t(VertexAttrib::RANGE_SIZE); ++i)
    {
        const AttributeProviderId pid = key.attribute_providers[i];
        if (pid == AttributeProviderId::None)
            continue;

        const AttributeProvider *p = FindBuiltinAttribProvider(pid);
        if (!p)
            continue;

        const VertexAttrib attrib = VertexAttrib(i);
        if (attrib < VertexAttrib::Position || attrib >= VertexAttrib::RANGE_SIZE || attrib == VertexAttrib::Position)
            continue;

        const uint32_t binding = uint32_t(attrib);

        const char *attrib_name = GetVertexAttribName(attrib);
        if (!attrib_name || !attrib_name[0])
            continue;

        out << "#define ATTRIB_TAG     " << attrib_name             << "\n";
        out << "#define ATTRIB_SET     " << kVertexStreamsSet        << "\n";
        out << "#define ATTRIB_BINDING " << binding                 << "\n";
        out << "#include \"" << p->glsl_path                        << "\"\n";
        out << "#undef ATTRIB_BINDING\n";
        out << "#undef ATTRIB_SET\n";
        out << "#undef ATTRIB_TAG\n";
    }
}

void EmitVertexStageInputs(std::ostream &out,
                           const mtl::MaterialVariantKey &key,
                           const PositionProvider &p,
                           int position_location)
{
    EmitPositionInput(out, p, position_location);
    EmitAttribInput(out, key);
}

}  // namespace hgl::graph
