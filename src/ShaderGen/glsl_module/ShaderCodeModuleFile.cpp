#include <hgl/mtl/ShaderCodeModuleFile.h>
#include <hgl/vk/VK.h>

#include <cstring>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    namespace
    {
        constexpr char ULRE_PREFIX[] = "// @ulre ";
        constexpr size_t ULRE_PREFIX_LENGTH = sizeof(ULRE_PREFIX) - 1;

        constexpr int TOKEN_CAPACITY = 128;

        const char *SkipSpaces(const char *cursor, const char *line_end)
        {
            while (cursor < line_end && (*cursor == ' ' || *cursor == '\t'))
                ++cursor;
            return cursor;
        }

        // Reads one whitespace-delimited token. Returns the position after the
        // token, or nullptr when no token is present. The token is written into
        // out (NUL-terminated, bounded by out_capacity).
        const char *ReadToken(const char *cursor, const char *line_end,
                              char *out, const int out_capacity)
        {
            cursor = SkipSpaces(cursor, line_end);
            if (cursor >= line_end)
                return nullptr;

            int written = 0;
            while (cursor < line_end && *cursor != ' ' && *cursor != '\t')
            {
                if (written + 1 < out_capacity)
                    out[written++] = *cursor;
                ++cursor;
            }
            out[written] = 0;
            return cursor;
        }

        bool ParseSignedInt(const char *token, int &out_value) noexcept
        {
            if (!token || !*token)
                return false;

            const char *cursor = token;
            bool negative = false;
            if (*cursor == '-')
            {
                negative = true;
                ++cursor;
            }
            else if (*cursor == '+')
            {
                ++cursor;
            }

            if (!*cursor)
                return false;

            uint64 value = 0;
            for (; *cursor; ++cursor)
            {
                if (*cursor < '0' || *cursor > '9')
                    return false;
                value = value * 10 + uint64(*cursor - '0');
                if (value > 0x7fffffff)
                    return false;
            }

            out_value = negative
                ? -int32(value)
                : int32(value);
            return true;
        }

        bool ParseUnsignedInt(const char *token, uint32 &out_value) noexcept
        {
            if (!token || !*token)
                return false;

            const char *cursor = token;
            int base = 10;
            if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))
            {
                base = 16;
                cursor += 2;
            }

            if (!*cursor)
                return false;

            uint64 value = 0;
            for (; *cursor; ++cursor)
            {
                const char c = *cursor;
                int digit;
                if (c >= '0' && c <= '9')
                    digit = c - '0';
                else if (c >= 'a' && c <= 'f')
                    digit = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F')
                    digit = c - 'A' + 10;
                else
                    return false;

                const uint64 base_value = base == 16 ? 16ull : 10ull;
                if (value > (0xffffffffull - uint64(digit)) / base_value)
                    return false;
                value = value * base_value + uint64(digit);
            }

            out_value = uint32(value);
            return true;
        }

        bool ParseKind(const char *token, ShaderCodeModuleKind &out_kind) noexcept
        {
            if (std::strcmp(token, "Shared") == 0) out_kind = ShaderCodeModuleKind::Shared;
            else if (std::strcmp(token, "Surface") == 0) out_kind = ShaderCodeModuleKind::Surface;
            else if (std::strcmp(token, "Position") == 0) out_kind = ShaderCodeModuleKind::Position;
            else if (std::strcmp(token, "Transform") == 0) out_kind = ShaderCodeModuleKind::Transform;
            else if (std::strcmp(token, "Utility") == 0) out_kind = ShaderCodeModuleKind::Utility;
            else if (std::strcmp(token, "FragmentShader") == 0) out_kind = ShaderCodeModuleKind::FragmentShader;
            else return false;
            return true;
        }

        ShaderCodeModuleSemantic ParseSemantic(const char *token) noexcept
        {
            // T1：语义名字单一真源 ShaderCodeModule.h——此处不再维护平行 if-else 表。
            ShaderCodeModuleSemantic semantic = ShaderCodeModuleSemantic::Unknown;
            ParseShaderCodeModuleSemantic(token, semantic);
            return semantic;    // 解析失败返回 Unknown（旧行为一致）
        }

        bool ParseSource(const char *token, ShaderCodeModuleCapabilitySource &out_source) noexcept
        {
            if (std::strcmp(token, "GeometryAttribute") == 0) out_source = ShaderCodeModuleCapabilitySource::GeometryAttribute;
            else if (std::strcmp(token, "Resource") == 0) out_source = ShaderCodeModuleCapabilitySource::Resource;
            else if (std::strcmp(token, "ProducedSemantic") == 0) out_source = ShaderCodeModuleCapabilitySource::ProducedSemantic;
            else return false;
            return true;
        }

        bool ParseNumericClassMask(const char *token, uint32 &out_mask) noexcept
        {
            uint32 mask = 0;
            const char *cursor = token;
            bool any_parsed = false;

            while (*cursor)
            {
                const char *separator = std::strchr(cursor, '|');
                const size_t length = separator
                    ? size_t(separator - cursor)
                    : std::strlen(cursor);

                char part[64];
                if (length >= sizeof(part))
                    return false;
                std::memcpy(part, cursor, length);
                part[length] = 0;

                if (std::strcmp(part, "Float") == 0)
                    mask |= uint32(ShaderCodeModuleNumericClass::Float);
                else if (std::strcmp(part, "SignedInteger") == 0)
                    mask |= uint32(ShaderCodeModuleNumericClass::SignedInteger);
                else if (std::strcmp(part, "UnsignedInteger") == 0)
                    mask |= uint32(ShaderCodeModuleNumericClass::UnsignedInteger);
                else if (std::strcmp(part, "Normalized") == 0)
                    mask |= uint32(ShaderCodeModuleNumericClass::Normalized);
                else if (std::strcmp(part, "Packed") == 0)
                    mask |= uint32(ShaderCodeModuleNumericClass::Packed);
                else if (std::strcmp(part, "Any") == 0)
                    any_parsed = true;
                else
                    return false;

                if (!separator)
                    break;
                cursor = separator + 1;
            }

            if (mask == 0 && !any_parsed)
                return false;

            out_mask = any_parsed
                ? uint32(ShaderCodeModuleNumericClass::Any)
                : mask;
            return true;
        }

        bool ParseStageFlags(const char *token, uint32 &out_flags) noexcept
        {
            if (!token || !*token)
                return false;

            if (ParseUnsignedInt(token, out_flags))
                return true;

            out_flags = 0;
            const char *cursor = token;
            while (*cursor)
            {
                const char *separator = std::strchr(cursor, '|');
                const size_t length = separator
                    ? size_t(separator - cursor) : std::strlen(cursor);
                char part[32];
                if (length == 0 || length >= sizeof(part))
                    return false;
                std::memcpy(part, cursor, length);
                part[length] = 0;

                uint32 bit = 0;
                if (std::strcmp(part, "Mesh") == 0)
                    bit = VK_SHADER_STAGE_MESH_BIT_EXT;
                else if (std::strcmp(part, "Fragment") == 0)
                    bit = VK_SHADER_STAGE_FRAGMENT_BIT;
                else if (std::strcmp(part, "Compute") == 0)
                    bit = VK_SHADER_STAGE_COMPUTE_BIT;
                else if (std::strcmp(part, "AllGraphics") == 0)
                    bit = VK_SHADER_STAGE_ALL_GRAPHICS;
                else if (std::strcmp(part, "MeshFragment") == 0)
                    bit = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
                else
                    return false;

                out_flags |= bit;
                if (!separator)
                    break;
                cursor = separator + 1;
            }

            return out_flags != 0;
        }

        bool ParseResourcePolicy(const char *token, bool &required, bool &fallback) noexcept
        {
            if (std::strcmp(token, "required") == 0)
                required = true;
            else if (std::strcmp(token, "optional") == 0)
                required = false;
            else if (std::strcmp(token, "fallback") == 0)
                fallback = true;
            else if (std::strcmp(token, "strict") == 0)
                fallback = false;
            else
                return false;
            return true;
        }
        bool ParseSSBOType(const char *token, SSBOType &out) noexcept
        {
            for (uint32 i = 0; i < static_cast<uint32>(SSBOType::RANGE_SIZE); ++i)
            {
                const SSBOType type = static_cast<SSBOType>(i);
                if (std::strcmp(token, GetSSBOTypeName(type)) == 0)
                {
                    out = type;
                    return true;
                }
            }
            return false;
        }

    }

    const char *GetShaderCodeModuleParseResultName(const ShaderCodeModuleParseResult result) noexcept
    {
        switch (result)
        {
            case ShaderCodeModuleParseResult::Skipped: return "Skipped";
            case ShaderCodeModuleParseResult::OK: return "OK";
            case ShaderCodeModuleParseResult::MissingBegin: return "MissingBegin";
            case ShaderCodeModuleParseResult::DuplicateBegin: return "DuplicateBegin";
            case ShaderCodeModuleParseResult::MissingEnd: return "MissingEnd";
            case ShaderCodeModuleParseResult::UnknownDirective: return "UnknownDirective";
            case ShaderCodeModuleParseResult::DuplicateDirective: return "DuplicateDirective";
            case ShaderCodeModuleParseResult::MissingDirectiveArgument: return "MissingDirectiveArgument";
            case ShaderCodeModuleParseResult::InvalidKind: return "InvalidKind";
            case ShaderCodeModuleParseResult::InvalidSemantic: return "InvalidSemantic";
            case ShaderCodeModuleParseResult::InvalidSource: return "InvalidSource";
            case ShaderCodeModuleParseResult::InvalidNumericClass: return "InvalidNumericClass";
            case ShaderCodeModuleParseResult::InvalidNumber: return "InvalidNumber";
            case ShaderCodeModuleParseResult::InvalidResource: return "InvalidResource";
            case ShaderCodeModuleParseResult::InvalidStage: return "InvalidStage";
            case ShaderCodeModuleParseResult::InvalidDependency: return "InvalidDependency";
            case ShaderCodeModuleParseResult::InvalidConflict: return "InvalidConflict";
            default: return "Unknown";
        }
    }

    ShaderCodeModuleParseResult ParseShaderCodeModuleFile(const char *content,
                                                      const int content_size,
                                                      ShaderCodeModuleFileData &out_data) noexcept
    {
        if (!content || content_size <= 0)
            return ShaderCodeModuleParseResult::Skipped;

        const char *end = content + content_size;
        const char *cursor = content;

        bool in_block = false;
        bool saw_any_ulre = false;
        bool saw_end = false;
        bool saw_kind = false;
        bool saw_priority = false;
        char token[TOKEN_CAPACITY];

        while (cursor < end && !saw_end)
        {
            const char *line_break = static_cast<const char *>(
                std::memchr(cursor, '\n', size_t(end - cursor)));
            const char *line_end = line_break ? line_break : end;

            if (line_end > cursor && line_end[-1] == '\r')
                --line_end;

            const ptrdiff_t line_length = line_end - cursor;
            if (line_length >= ptrdiff_t(ULRE_PREFIX_LENGTH)
             && std::memcmp(cursor, ULRE_PREFIX, ULRE_PREFIX_LENGTH) == 0)
            {
                saw_any_ulre = true;

                const char *directive_cursor = cursor + ULRE_PREFIX_LENGTH;
                const char *after_keyword = ReadToken(directive_cursor, line_end, token, sizeof(token));

                if (!after_keyword || !token[0])
                {
                    if (!in_block)
                        return ShaderCodeModuleParseResult::MissingBegin;
                    return ShaderCodeModuleParseResult::UnknownDirective;
                }

                if (std::strcmp(token, "begin") == 0)
                {
                    if (in_block)
                        return ShaderCodeModuleParseResult::DuplicateBegin;
                    in_block = true;
                }
                else if (std::strcmp(token, "end") == 0)
                {
                    if (!in_block)
                        return ShaderCodeModuleParseResult::MissingBegin;
                    in_block = false;
                    saw_end = true;
                }
                else if (!in_block)
                {
                    return ShaderCodeModuleParseResult::MissingBegin;
                }
                else if (std::strcmp(token, "name") == 0)
                {
                    if (!out_data.name.IsEmpty())
                        return ShaderCodeModuleParseResult::DuplicateDirective;

                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return ShaderCodeModuleParseResult::MissingDirectiveArgument;
                    out_data.name = AnsiString(token);
                }
                else if (std::strcmp(token, "kind") == 0)
                {
                    if (saw_kind)
                        return ShaderCodeModuleParseResult::DuplicateDirective;

                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return ShaderCodeModuleParseResult::MissingDirectiveArgument;

                    ShaderCodeModuleKind kind;
                    if (!ParseKind(token, kind))
                        return ShaderCodeModuleParseResult::InvalidKind;
                    out_data.kind = kind;
                    saw_kind = true;
                }
                else if (std::strcmp(token, "priority") == 0)
                {
                    if (saw_priority)
                        return ShaderCodeModuleParseResult::DuplicateDirective;

                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return ShaderCodeModuleParseResult::MissingDirectiveArgument;

                    int value = 0;
                    if (!ParseSignedInt(token, value))
                        return ShaderCodeModuleParseResult::InvalidNumber;
                    out_data.priority = value;
                    saw_priority = true;
                }
                else if (std::strcmp(token, "ssbo") == 0)
                {
                    ShaderCodeModuleSSBORequirement requirement;
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return ShaderCodeModuleParseResult::MissingDirectiveArgument;
                    AnsiString *ssbo_name = out_data.ssbo_name_storage.Create();
                    if (!ssbo_name)
                        return ShaderCodeModuleParseResult::InvalidResource;
                    *ssbo_name = token;
                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !ParseSSBOType(token, requirement.ssbo_type))
                        return ShaderCodeModuleParseResult::InvalidResource;
                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !ParseUnsignedInt(token, requirement.material_private_data_slot))
                        return ShaderCodeModuleParseResult::InvalidNumber;
                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !ParseStageFlags(token, requirement.stage_flags))
                        return ShaderCodeModuleParseResult::InvalidStage;

                    while ((next = ReadToken(next, line_end, token, sizeof(token))) != nullptr)
                    {
                        if (!ParseResourcePolicy(token, requirement.required,
                                                  requirement.allow_fallback))
                            return ShaderCodeModuleParseResult::InvalidResource;
                    }
                    out_data.ssbo_requirements.Add(requirement);
                }
                else if (std::strcmp(token, "texture_layer") == 0)
                {
                    ShaderCodeModuleTextureLayerRequirement requirement;
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !ParseTextureSlotName(token, requirement.slot))
                        return ShaderCodeModuleParseResult::InvalidResource;
                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !ParseStageFlags(token, requirement.stage_flags))
                        return ShaderCodeModuleParseResult::InvalidStage;

                    while ((next = ReadToken(next, line_end, token, sizeof(token))) != nullptr)
                    {
                        if (!ParseResourcePolicy(token, requirement.required,
                                                  requirement.allow_fallback))
                            return ShaderCodeModuleParseResult::InvalidResource;
                    }
                    out_data.texture_layer_requirements.Add(requirement);
                }
                else if (std::strcmp(token, "require") == 0)
                {
                    ShaderCodeModuleSemanticRequirement requirement;

                    // <source> <semantic> [<numclass> [<min> [<max>]]]
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return ShaderCodeModuleParseResult::MissingDirectiveArgument;
                    if (!ParseSource(token, requirement.source))
                        return ShaderCodeModuleParseResult::InvalidSource;

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return ShaderCodeModuleParseResult::MissingDirectiveArgument;
                    requirement.semantic = ParseSemantic(token);
                    if (requirement.semantic == ShaderCodeModuleSemantic::Unknown)
                        return ShaderCodeModuleParseResult::InvalidSemantic;

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (next && token[0])
                    {
                        uint32 mask = 0;
                        if (!ParseNumericClassMask(token, mask))
                            return ShaderCodeModuleParseResult::InvalidNumericClass;
                        requirement.numeric_class_mask = mask;

                        next = ReadToken(next, line_end, token, sizeof(token));
                        if (next && token[0])
                        {
                            uint32 min_components = 0;
                            if (!ParseUnsignedInt(token, min_components))
                                return ShaderCodeModuleParseResult::InvalidNumber;
                            requirement.min_component_count = uint8(min_components);

                            next = ReadToken(next, line_end, token, sizeof(token));
                            if (next && token[0])
                            {
                                uint32 max_components = 0;
                                if (!ParseUnsignedInt(token, max_components))
                                    return ShaderCodeModuleParseResult::InvalidNumber;
                                requirement.max_component_count = uint8(max_components);
                            }
                        }
                    }

                    out_data.semantic_requirements.Add(requirement);
                }
                else if (std::strcmp(token, "provide") == 0)
                {
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return ShaderCodeModuleParseResult::MissingDirectiveArgument;

                    const ShaderCodeModuleSemantic semantic = ParseSemantic(token);
                    if (semantic == ShaderCodeModuleSemantic::Unknown)
                        return ShaderCodeModuleParseResult::InvalidSemantic;
                    out_data.semantic_provides.Add(semantic);
                }
                else if (std::strcmp(token, "uses") == 0)
                {
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return ShaderCodeModuleParseResult::MissingDirectiveArgument;

                    const AnsiString dependency_name(token);
                    ShaderCodeModuleDependency dependency{};
                    dependency.module_name = dependency_name.c_str();

                    out_data.pending_module_requirements.Add(dependency_name);
                    out_data.pending_dependency_versions.Add(dependency);
                }
                else if (std::strcmp(token, "conflicts") == 0)
                {
                    const char *next =
                        ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return ShaderCodeModuleParseResult::InvalidConflict;

                    out_data.pending_module_conflicts.Add(AnsiString(token));
                }
                else
                {
                    return ShaderCodeModuleParseResult::UnknownDirective;
                }
            }

            cursor = line_break ? line_break + 1 : end;
        }

        if (!saw_any_ulre)
            return ShaderCodeModuleParseResult::Skipped;

        if (in_block)
            return ShaderCodeModuleParseResult::MissingEnd;

        return ShaderCodeModuleParseResult::OK;
    }
}
