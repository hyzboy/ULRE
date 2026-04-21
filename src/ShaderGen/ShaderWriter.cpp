#include <hgl/shadergen/ShaderWriter.h>

#include <string_view>

namespace hgl::graph
{
namespace
{
constexpr size_t kDebugCommentColumn = 100;

std::string_view GetFileNameOnly(const std::string_view path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}
}

ShaderWriter::ShaderWriter(std::string &output)
    : out(output), indent_level(0)
{
}

void ShaderWriter::FlushLayoutPrefix()
{
    if (!pending_layout_prefix.empty())
    {
        out += pending_layout_prefix;
        pending_layout_prefix.clear();
    }
}

ShaderWriter &ShaderWriter::EmitLayoutBinding(uint32_t set, uint32_t binding)
{
    pending_layout_prefix = "layout(set=";
    pending_layout_prefix += std::to_string(set);
    pending_layout_prefix += ", binding=";
    pending_layout_prefix += std::to_string(binding);
    pending_layout_prefix += ") ";
    return *this;
}

ShaderWriter &ShaderWriter::EmitLayoutLocation(uint32_t location)
{
    pending_layout_prefix = "layout(location=";
    pending_layout_prefix += std::to_string(location);
    pending_layout_prefix += ") ";
    return *this;
}

ShaderWriter &ShaderWriter::EmitUniform(const char *type, const char *name)
{
    FlushLayoutPrefix();
    out += "uniform ";
    out += type;
    out += ' ';
    out += name;
    out += ";\n";
    return *this;
}

ShaderWriter &ShaderWriter::EmitVariable(const char *type, const std::string &name)
{
    FlushLayoutPrefix();
    out += type;
    out += ' ';
    out += name;
    out += ";\n";
    return *this;
}

ShaderWriter &ShaderWriter::EmitInOut(const char *qualifier, const char *type, const char *name)
{
    FlushLayoutPrefix();
    out += qualifier;
    out += ' ';
    out += type;
    out += ' ';
    out += name;
    out += ";\n";
    return *this;
}

ShaderWriter &ShaderWriter::EmitDefine(const std::string &macro, const char *value)
{
    out += "#define ";
    out += macro;

    if (value && value[0])
    {
        out += ' ';
        out += value;
    }

    out += '\n';
    return *this;
}

ShaderWriter &ShaderWriter::EmitIfndefDef(const std::string &macro, const char *value)
{
    out += "#ifndef ";
    out += macro;
    out += "\n#define ";
    out += macro;

    if (value && value[0])
    {
        out += ' ';
        out += value;
    }

    out += "\n#endif\n";
    return *this;
}

ShaderWriter &ShaderWriter::EmitInclude(const std::string &path)
{
    out += "#include \"";
    out += path;
    out += "\"\n";
    return *this;
}

ShaderWriter &ShaderWriter::EmitCommentLine(const std::string &name, const std::source_location &location)
{
    FlushLayoutPrefix();

    if (indent_level > 0)
        out.append(static_cast<size_t>(indent_level) * 4, ' ');

    out += "// ";
    out += name;
    out += ' ';

    std::string suffix;
    suffix.reserve(192);
    suffix += ' ';
    suffix += std::string(GetFileNameOnly(location.file_name()));
    suffix += " | line ";
    suffix += std::to_string(location.line());

    const size_t current_width = name.size() + 3;
    const size_t dash_count = current_width < kDebugCommentColumn
                            ? (kDebugCommentColumn - current_width)
                            : 1;

    out.append(dash_count, '-');
    out += suffix;
    out += '\n';
    return *this;
}

ShaderWriter &ShaderWriter::EmitLine(const std::string &line)
{
    FlushLayoutPrefix();
    if (indent_level > 0)
        out.append(static_cast<size_t>(indent_level) * 4, ' ');
    out += line;
    out += '\n';
    return *this;
}

ShaderWriter &ShaderWriter::BeginBlock()
{
    FlushLayoutPrefix();
    if (indent_level > 0)
        out.append(static_cast<size_t>(indent_level) * 4, ' ');
    out += "{\n";
    ++indent_level;
    return *this;
}

ShaderWriter &ShaderWriter::EndBlock(const char *trailing)
{
    FlushLayoutPrefix();
    if (indent_level > 0)
        --indent_level;
    if (indent_level > 0)
        out.append(static_cast<size_t>(indent_level) * 4, ' ');
    out += '}';
    if (trailing && trailing[0])
    {
        if (trailing[0] != ';')
            out += ' ';
        out += trailing;
    }
    out += '\n';
    return *this;
}

ShaderWriter &ShaderWriter::NewLine()
{
    FlushLayoutPrefix();
    out += '\n';
    return *this;
}
}