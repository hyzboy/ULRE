#include <hgl/mtl/GlslModuleFile.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/vk/VK.h>

#include <cstring>

namespace hgl::graph::mtl
{
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

        bool ParseSlotRole(
            const char *token, ShaderModuleSlotRole &out_role) noexcept
        {
            // 上界用 SlotRoleCount 哨兵：新增 role 时无需改动此处。
            for (uint32 value = uint32(ShaderModuleSlotRole::Unknown) + 1;
                 value < uint32(ShaderModuleSlotRole::SlotRoleCount);
                 ++value)
            {
                const ShaderModuleSlotRole role = ShaderModuleSlotRole(value);
                if (std::strcmp(token, GetShaderModuleSlotRoleName(role)) == 0)
                {
                    out_role = role;
                    return true;
                }
            }
            return false;
        }

        bool ParseCapabilityMask(
            const char *token, uint32 &out_mask) noexcept
        {
            static const struct
            {
                const char *name;
                ShaderModuleCapability capability;
            } capabilities[] =
            {
                { "surface_base_color", ShaderModuleCapability::SurfaceBaseColor },
                { "surface_normal", ShaderModuleCapability::SurfaceNormal },
                { "surface_metallic", ShaderModuleCapability::SurfaceMetallic },
                { "surface_roughness", ShaderModuleCapability::SurfaceRoughness },
                { "surface_opacity", ShaderModuleCapability::SurfaceOpacity },
                { "direct_light", ShaderModuleCapability::DirectLight },
                { "direct_diffuse", ShaderModuleCapability::DirectDiffuse },
                { "direct_specular", ShaderModuleCapability::DirectSpecular },
                { "shadow_factor", ShaderModuleCapability::ShadowFactor },
                { "ambient_diffuse", ShaderModuleCapability::AmbientDiffuse },
                { "ambient_specular", ShaderModuleCapability::AmbientSpecular },
                { "ambient_occlusion", ShaderModuleCapability::AmbientOcclusion },
                { "forward_output", ShaderModuleCapability::ForwardOutput }
            };

            out_mask = 0;
            const char *begin = token;
            while (begin && *begin)
            {
                const char *end = std::strchr(begin, '|');
                const size_t length = end ? size_t(end - begin) : std::strlen(begin);
                bool found = false;
                for (const auto &entry : capabilities)
                {
                    if (std::strlen(entry.name) == length
                     && std::strncmp(begin, entry.name, length) == 0)
                    {
                        out_mask |= uint32(entry.capability);
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return false;
                begin = end ? end + 1 : nullptr;
            }
            return out_mask != 0;
        }

        GlslModuleSemantic ParseSemantic(const char *token) noexcept
        {
            // T1：语义名字单一真源 GlslModule.h——此处不再维护平行 if-else 表。
            GlslModuleSemantic semantic = GlslModuleSemantic::Unknown;
            ParseGlslModuleSemantic(token, semantic);
            return semantic;    // 解析失败返回 Unknown（旧行为一致）
        }

        bool ParseSource(const char *token, GlslModuleCapabilitySource &out_source) noexcept
        {
            if (std::strcmp(token, "GeometryAttribute") == 0) out_source = GlslModuleCapabilitySource::GeometryAttribute;
            else if (std::strcmp(token, "Resource") == 0) out_source = GlslModuleCapabilitySource::Resource;
            else if (std::strcmp(token, "ProducedSemantic") == 0) out_source = GlslModuleCapabilitySource::ProducedSemantic;
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
                    mask |= uint32(GlslModuleNumericClass::Float);
                else if (std::strcmp(part, "SignedInteger") == 0)
                    mask |= uint32(GlslModuleNumericClass::SignedInteger);
                else if (std::strcmp(part, "UnsignedInteger") == 0)
                    mask |= uint32(GlslModuleNumericClass::UnsignedInteger);
                else if (std::strcmp(part, "Normalized") == 0)
                    mask |= uint32(GlslModuleNumericClass::Normalized);
                else if (std::strcmp(part, "Packed") == 0)
                    mask |= uint32(GlslModuleNumericClass::Packed);
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
                ? uint32(GlslModuleNumericClass::Any)
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
    }

    const char *GetGlslModuleParseResultName(const GlslModuleParseResult result) noexcept
    {
        switch (result)
        {
            case GlslModuleParseResult::Skipped: return "Skipped";
            case GlslModuleParseResult::OK: return "OK";
            case GlslModuleParseResult::MissingBegin: return "MissingBegin";
            case GlslModuleParseResult::DuplicateBegin: return "DuplicateBegin";
            case GlslModuleParseResult::MissingEnd: return "MissingEnd";
            case GlslModuleParseResult::UnknownDirective: return "UnknownDirective";
            case GlslModuleParseResult::DuplicateDirective: return "DuplicateDirective";
            case GlslModuleParseResult::MissingDirectiveArgument: return "MissingDirectiveArgument";
            case GlslModuleParseResult::InvalidKind: return "InvalidKind";
            case GlslModuleParseResult::InvalidSemantic: return "InvalidSemantic";
            case GlslModuleParseResult::InvalidSource: return "InvalidSource";
            case GlslModuleParseResult::InvalidNumericClass: return "InvalidNumericClass";
            case GlslModuleParseResult::InvalidNumber: return "InvalidNumber";
            case GlslModuleParseResult::InvalidResource: return "InvalidResource";
            case GlslModuleParseResult::InvalidStage: return "InvalidStage";
            case GlslModuleParseResult::InvalidSlotRole: return "InvalidSlotRole";
            case GlslModuleParseResult::InvalidCapability: return "InvalidCapability";
            case GlslModuleParseResult::InvalidDependency: return "InvalidDependency";
            default: return "Unknown";
        }
    }

    GlslModuleParseResult ParseGlslModuleFile(const char *content,
                                                      const int content_size,
                                                      GlslModuleFileData &out_data) noexcept
    {
        if (!content || content_size <= 0)
            return GlslModuleParseResult::Skipped;

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
                        return GlslModuleParseResult::MissingBegin;
                    return GlslModuleParseResult::UnknownDirective;
                }

                if (std::strcmp(token, "begin") == 0)
                {
                    if (in_block)
                        return GlslModuleParseResult::DuplicateBegin;
                    in_block = true;
                }
                else if (std::strcmp(token, "end") == 0)
                {
                    if (!in_block)
                        return GlslModuleParseResult::MissingBegin;
                    in_block = false;
                    saw_end = true;
                }
                else if (!in_block)
                {
                    return GlslModuleParseResult::MissingBegin;
                }
                else if (std::strcmp(token, "name") == 0)
                {
                    if (!out_data.name.IsEmpty())
                        return GlslModuleParseResult::DuplicateDirective;

                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GlslModuleParseResult::MissingDirectiveArgument;
                    out_data.name = AnsiString(token);
                }
                else if (std::strcmp(token, "kind") == 0)
                {
                    if (saw_kind)
                        return GlslModuleParseResult::DuplicateDirective;

                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GlslModuleParseResult::MissingDirectiveArgument;

                    GlslModuleKind kind;
                    if (!ParseGlslModuleKind(token, kind))
                        return GlslModuleParseResult::InvalidKind;
                    out_data.kind = kind;
                    saw_kind = true;
                }
                else if (std::strcmp(token, "priority") == 0)
                {
                    if (saw_priority)
                        return GlslModuleParseResult::DuplicateDirective;

                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GlslModuleParseResult::MissingDirectiveArgument;

                    int value = 0;
                    if (!ParseSignedInt(token, value))
                        return GlslModuleParseResult::InvalidNumber;
                    out_data.priority = value;
                    saw_priority = true;
                }
                else if (std::strcmp(token, "slot") == 0)
                {
                    if (out_data.slot_role != ShaderModuleSlotRole::Unknown)
                        return GlslModuleParseResult::DuplicateDirective;
                    const char *next =
                        ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !ParseSlotRole(token, out_data.slot_role))
                        return GlslModuleParseResult::InvalidSlotRole;
                }
                else if (std::strcmp(token, "provides_capability") == 0
                      || std::strcmp(token, "requires_capability") == 0)
                {
                    const bool provides =
                        std::strcmp(token, "provides_capability") == 0;
                    uint32 capabilities = 0;
                    const char *next =
                        ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !ParseCapabilityMask(token, capabilities))
                        return GlslModuleParseResult::InvalidCapability;
                    uint32 &target =
                        provides
                            ? out_data.provided_capabilities
                            : out_data.required_capabilities;
                    if (target != 0)
                        return GlslModuleParseResult::DuplicateDirective;
                    target = capabilities;
                }
                else if (std::strcmp(token, "texture_reference") == 0)
                {
                    GlslModuleTextureReferenceRequirement
                        requirement;
                    const char *next =
                        ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !IsValidMaterialTextureName(token))
                        return GlslModuleParseResult::InvalidResource;

                    AnsiString *texture_name =
                        out_data.texture_reference_name_storage.Create();
                    if (!texture_name)
                        return GlslModuleParseResult::InvalidResource;
                    *texture_name = token;
                    requirement.texture_name = texture_name->c_str();

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !ParseStageFlags(token, requirement.stage_flags))
                        return GlslModuleParseResult::InvalidStage;

                    while ((next =
                                ReadToken(next, line_end, token, sizeof(token)))
                           != nullptr)
                    {
                        if (!ParseResourcePolicy(
                                token,
                                requirement.required,
                                requirement.allow_fallback))
                            return GlslModuleParseResult::InvalidResource;
                    }
                    out_data.texture_reference_requirements.Add(requirement);
                }
                else if (std::strcmp(token, "sampler_reference") == 0)
                {
                    // <sampler-preset-name> [required|optional]
                    GlslModuleSamplerReferenceRequirement requirement;
                    const char *next =
                        ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !IsValidMaterialTextureName(token))
                        return GlslModuleParseResult::InvalidResource;

                    AnsiString *sampler_name =
                        out_data.sampler_reference_name_storage.Create();
                    if (!sampler_name)
                        return GlslModuleParseResult::InvalidResource;
                    *sampler_name = token;
                    requirement.sampler_name = sampler_name->c_str();

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (next && token[0])
                    {
                        if (std::strcmp(token, "optional") == 0)
                            requirement.required = false;
                        else if (std::strcmp(token, "required") != 0)
                            return GlslModuleParseResult::InvalidResource;
                    }
                    out_data.sampler_reference_requirements.Add(requirement);
                }
                else if (std::strcmp(token, "require") == 0)
                {
                    GlslModuleSemanticRequirement requirement;

                    // <source> <semantic> [<numclass> [<min> [<max>]]]
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GlslModuleParseResult::MissingDirectiveArgument;
                    if (!ParseSource(token, requirement.source))
                        return GlslModuleParseResult::InvalidSource;

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GlslModuleParseResult::MissingDirectiveArgument;
                    requirement.semantic = ParseSemantic(token);
                    if (requirement.semantic == GlslModuleSemantic::Unknown)
                        return GlslModuleParseResult::InvalidSemantic;

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (next && token[0])
                    {
                        uint32 mask = 0;
                        if (!ParseNumericClassMask(token, mask))
                            return GlslModuleParseResult::InvalidNumericClass;
                        requirement.numeric_class_mask = mask;

                        next = ReadToken(next, line_end, token, sizeof(token));
                        if (next && token[0])
                        {
                            uint32 min_components = 0;
                            if (!ParseUnsignedInt(token, min_components))
                                return GlslModuleParseResult::InvalidNumber;
                            requirement.min_component_count = uint8(min_components);

                            next = ReadToken(next, line_end, token, sizeof(token));
                            if (next && token[0])
                            {
                                uint32 max_components = 0;
                                if (!ParseUnsignedInt(token, max_components))
                                    return GlslModuleParseResult::InvalidNumber;
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
                        return GlslModuleParseResult::MissingDirectiveArgument;

                    const GlslModuleSemantic semantic = ParseSemantic(token);
                    if (semantic == GlslModuleSemantic::Unknown)
                        return GlslModuleParseResult::InvalidSemantic;
                    out_data.semantic_provides.Add(semantic);
                }
                else if (std::strcmp(token, "uses") == 0)
                {
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GlslModuleParseResult::MissingDirectiveArgument;

                    out_data.pending_module_requirements.Add(AnsiString(token));
                }
                else
                {
                    return GlslModuleParseResult::UnknownDirective;
                }
            }

            cursor = line_break ? line_break + 1 : end;
        }

        if (!saw_any_ulre)
            return GlslModuleParseResult::Skipped;

        if (in_block)
            return GlslModuleParseResult::MissingEnd;

        return GlslModuleParseResult::OK;
    }

    int CollectGlslModuleIncludePaths(const char *content,
                                           const int content_size,
                                           AnsiStringList &out_paths) noexcept
    {
        if (!content || content_size <= 0)
            return 0;

        const int begin_count = out_paths.GetCount();
        const char *end = content + content_size;
        const char *cursor = content;

        while (cursor < end)
        {
            const char *line_break = static_cast<const char *>(
                std::memchr(cursor, '\n', size_t(end - cursor)));
            const char *line_end = line_break ? line_break : end;

            const char *scan = SkipSpaces(cursor, line_end);

            // `#include` 必须位于行首（前导空白除外）：以 `//` 或 `/*` 开头的
            // 注释行因此天然被排除，不会把被注释掉的 include 当成依赖。
            if (line_end - scan > 8
             && std::strncmp(scan, "#include", 8) == 0)
            {
                scan = SkipSpaces(scan + 8, line_end);

                if (scan < line_end && (*scan == '"' || *scan == '<'))
                {
                    const char closing = *scan == '"' ? '"' : '>';
                    const char *name_begin = scan + 1;
                    const char *name_end = name_begin;
                    while (name_end < line_end && *name_end != closing)
                        ++name_end;

                    if (name_end > name_begin)
                        out_paths.Add(
                            AnsiString(name_begin, int(name_end - name_begin)));
                }
            }

            cursor = line_break ? line_break + 1 : end;
        }

        return out_paths.GetCount() - begin_count;
    }
}
