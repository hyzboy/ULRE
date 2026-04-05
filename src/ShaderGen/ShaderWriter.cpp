#include <hgl/shadergen/ShaderWriter.h>

namespace hgl::graph
{
ShaderWriter::ShaderWriter(std::string &output)
    : out(output)
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

ShaderWriter &ShaderWriter::EmitLine(const std::string &line)
{
    FlushLayoutPrefix();
    out += line;
    out += '\n';
    return *this;
}

ShaderWriter &ShaderWriter::BeginBlock()
{
    FlushLayoutPrefix();
    out += "{\n";
    return *this;
}

ShaderWriter &ShaderWriter::EndBlock()
{
    FlushLayoutPrefix();
    out += "}\n";
    return *this;
}

ShaderWriter &ShaderWriter::NewLine()
{
    FlushLayoutPrefix();
    out += '\n';
    return *this;
}
}