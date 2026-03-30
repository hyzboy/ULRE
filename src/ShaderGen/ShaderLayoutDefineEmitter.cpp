/// ShaderLayoutDefineEmitter.cpp
///
/// Converts a ShaderLayoutContract into a GLSL #define block that can be
/// prepended to any shader stage source.

#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <string>

namespace hgl::graph
{

static void AppendSection(std::string       &out,
                          const char        *section_comment,
                          const std::vector<ShaderLayoutEntry> &entries)
{
    if (entries.empty())
        return;

    out += "// ";
    out += section_comment;
    out += '\n';

    for (const ShaderLayoutEntry &e : entries)
    {
        out += "#define ";
        out += e.macro_name;
        out += ' ';
        out += std::to_string(e.value);
        out += '\n';
    }
}

std::string EmitShaderLayoutDefines(const ShaderLayoutContract &contract)
{
    if (contract.Empty())
        return {};

    std::string out;
    out.reserve(512);

    out += "// ---- Auto-generated layout defines ----\n";

    AppendSection(out, "Vertex input locations", contract.vertex_locations);
    AppendSection(out, "Descriptor sets",        contract.descriptor_sets);
    AppendSection(out, "Descriptor bindings",    contract.descriptor_bindings);

    out += "// ----------------------------------------\n\n";
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
