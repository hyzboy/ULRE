#pragma once

#include <cstdint>
#include <string>

namespace hgl::graph
{
class ShaderWriter
{
    std::string &out;
    std::string pending_layout_prefix;
    int indent_level;

    void FlushLayoutPrefix();

public:
    explicit ShaderWriter(std::string &output);

    ShaderWriter &EmitLayoutBinding(uint32_t set, uint32_t binding);
    ShaderWriter &EmitLayoutLocation(uint32_t location);

    ShaderWriter &EmitUniform(const char *type, const char *name);
    ShaderWriter &EmitVariable(const char *type, const std::string &name);
    ShaderWriter &EmitInOut(const char *qualifier, const char *type, const char *name);

    ShaderWriter &EmitDefine(const std::string &macro, const char *value = nullptr);
    ShaderWriter &EmitIfndefDef(const std::string &macro, const char *value);
    ShaderWriter &EmitInclude(const std::string &path);

    ShaderWriter &EmitLine(const std::string &line);
    ShaderWriter &BeginBlock();
    // trailing=nullptr → "}", trailing=";" → "};", trailing="mit;" → "} mit;"
    ShaderWriter &EndBlock(const char *trailing = nullptr);
    ShaderWriter &NewLine();
};
}