/// ShaderLayoutEmitter.cpp
///
/// Converts a ShaderLayoutContract into a GLSL #define block that can be
/// prepended to any shader stage source.

#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/shadergen/ShaderWriter.h>
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

    writer.EmitLine("// ---- Auto-generated layout defines ----");

    AppendSection(out, "Vertex input locations", contract.vertex_locations);
    AppendSection(out, "Descriptor sets",        contract.descriptor_sets);
    AppendSection(out, "Descriptor bindings",    contract.descriptor_bindings);

    writer.EmitLine("// ----------------------------------------").NewLine();
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

}  // namespace hgl::graph
