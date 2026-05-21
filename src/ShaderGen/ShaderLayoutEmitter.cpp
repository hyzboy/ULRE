/// ShaderLayoutEmitter.cpp
///
/// Converts a ShaderLayoutContract into a GLSL #define block that can be
/// prepended to any shader stage source.

#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/shadergen/ShaderWriter.h>
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
    if (p.id == PositionProviderId::VAB_Vec3)
    {
        out << "layout(location=" << position_location << ") in vec3 inPosition;\n";
        out << "#define GetPositionLocal() (inPosition)\n";
    }
    else if (!p.glsl_path.empty())
    {
        if (p.vab_count > 0)
            out << "#define POSITION_LOCATION " << position_location << "\n";
        out << "#include \"" << p.glsl_path << "\"\n";
    }
    else
    {
        // Placeholder ID with no .glsl yet — should never reach emission.
        // CompositorAssembler performs route-time fallback before calling here.
        out << "// ERROR: position provider 0x" << std::hex
            << static_cast<unsigned>(p.id) << std::dec
            << " has no glsl_path (placeholder)\n";
    }
}

}  // namespace hgl::graph
