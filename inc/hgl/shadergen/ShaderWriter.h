#pragma once

#include <cstdint>
#include <source_location>
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
    ShaderWriter &EmitCommentLine(const std::string &name,
                                  const std::source_location &location = std::source_location::current());

    ShaderWriter &EmitLine(const std::string &line);
    ShaderWriter &BeginBlock();

    enum class EndBlockMode : uint8_t
    {
        Plain         = 0,  ///< 仅 '}'
        Statement     = 1,  ///< '};'，结构体/匿名块结尾
        NamedInstance = 2,  ///< '} <name>;'，需要传 instance_name
    };

    ShaderWriter &EndBlock(EndBlockMode mode = EndBlockMode::Plain);
    ShaderWriter &EndBlock(EndBlockMode mode, const std::string &instance_name);

    // 兼容旧调用，过渡期保留
    [[deprecated("Use EndBlock(EndBlockMode) or EndBlock(EndBlockMode, instance_name)")]]
    ShaderWriter &EndBlock(const char *trailing);

    ShaderWriter &NewLine();
};
}
