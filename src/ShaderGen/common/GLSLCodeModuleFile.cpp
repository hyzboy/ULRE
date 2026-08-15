#include <hgl/graph/glsl/GLSLCodeModuleFile.h>
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

        bool ParseKind(const char *token, GLSLCodeModuleKind &out_kind) noexcept
        {
            if (std::strcmp(token, "Shared") == 0) out_kind = GLSLCodeModuleKind::Shared;
            else if (std::strcmp(token, "Surface") == 0) out_kind = GLSLCodeModuleKind::Surface;
            else if (std::strcmp(token, "VertexInput") == 0) out_kind = GLSLCodeModuleKind::VertexInput;
            else if (std::strcmp(token, "Position") == 0) out_kind = GLSLCodeModuleKind::Position;
            else if (std::strcmp(token, "Basis") == 0) out_kind = GLSLCodeModuleKind::Basis;
            else if (std::strcmp(token, "Decode") == 0) out_kind = GLSLCodeModuleKind::Decode;
            else if (std::strcmp(token, "Transform") == 0) out_kind = GLSLCodeModuleKind::Transform;
            else if (std::strcmp(token, "Utility") == 0) out_kind = GLSLCodeModuleKind::Utility;
            else if (std::strcmp(token, "FragmentShader") == 0) out_kind = GLSLCodeModuleKind::FragmentShader;
            else return false;
            return true;
        }

        GLSLCodeModuleSemantic ParseSemantic(const char *token) noexcept
        {
            if (std::strcmp(token, "Position") == 0) return GLSLCodeModuleSemantic::Position;
            if (std::strcmp(token, "UV0") == 0) return GLSLCodeModuleSemantic::UV0;
            if (std::strcmp(token, "Color") == 0) return GLSLCodeModuleSemantic::Color;
            if (std::strcmp(token, "ColorY") == 0) return GLSLCodeModuleSemantic::ColorY;
            if (std::strcmp(token, "ColorUV") == 0) return GLSLCodeModuleSemantic::ColorUV;
            if (std::strcmp(token, "Normal") == 0) return GLSLCodeModuleSemantic::Normal;
            if (std::strcmp(token, "Tangent") == 0) return GLSLCodeModuleSemantic::Tangent;
            if (std::strcmp(token, "Binormal") == 0) return GLSLCodeModuleSemantic::Binormal;
            if (std::strcmp(token, "WorldPosition") == 0) return GLSLCodeModuleSemantic::WorldPosition;
            if (std::strcmp(token, "WorldNormal") == 0) return GLSLCodeModuleSemantic::WorldNormal;
            if (std::strcmp(token, "WorldTangent") == 0) return GLSLCodeModuleSemantic::WorldTangent;
            if (std::strcmp(token, "WorldBinormal") == 0) return GLSLCodeModuleSemantic::WorldBinormal;
            if (std::strcmp(token, "Luminance") == 0) return GLSLCodeModuleSemantic::Luminance;
            if (std::strcmp(token, "HeightMap") == 0) return GLSLCodeModuleSemantic::HeightMap;
            if (std::strcmp(token, "Camera") == 0) return GLSLCodeModuleSemantic::Camera;
            if (std::strcmp(token, "Viewport") == 0) return GLSLCodeModuleSemantic::Viewport;
            if (std::strcmp(token, "SkyLight") == 0) return GLSLCodeModuleSemantic::SkyLight;
            if (std::strcmp(token, "MaterialData") == 0) return GLSLCodeModuleSemantic::MaterialData;
            if (std::strcmp(token, "TransformID") == 0) return GLSLCodeModuleSemantic::TransformID;
            return GLSLCodeModuleSemantic::Unknown;
        }

        bool ParseSource(const char *token, GLSLCodeModuleCapabilitySource &out_source) noexcept
        {
            if (std::strcmp(token, "GeometryAttribute") == 0) out_source = GLSLCodeModuleCapabilitySource::GeometryAttribute;
            else if (std::strcmp(token, "Resource") == 0) out_source = GLSLCodeModuleCapabilitySource::Resource;
            else if (std::strcmp(token, "Option") == 0) out_source = GLSLCodeModuleCapabilitySource::Option;
            else if (std::strcmp(token, "ProducedSemantic") == 0) out_source = GLSLCodeModuleCapabilitySource::ProducedSemantic;
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
                    mask |= uint32(GLSLCodeModuleNumericClass::Float);
                else if (std::strcmp(part, "SignedInteger") == 0)
                    mask |= uint32(GLSLCodeModuleNumericClass::SignedInteger);
                else if (std::strcmp(part, "UnsignedInteger") == 0)
                    mask |= uint32(GLSLCodeModuleNumericClass::UnsignedInteger);
                else if (std::strcmp(part, "Normalized") == 0)
                    mask |= uint32(GLSLCodeModuleNumericClass::Normalized);
                else if (std::strcmp(part, "Packed") == 0)
                    mask |= uint32(GLSLCodeModuleNumericClass::Packed);
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
                ? uint32(GLSLCodeModuleNumericClass::Any)
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
                if (std::strcmp(part, "Vertex") == 0)
                    bit = VK_SHADER_STAGE_VERTEX_BIT;
                else if (std::strcmp(part, "Fragment") == 0)
                    bit = VK_SHADER_STAGE_FRAGMENT_BIT;
                else if (std::strcmp(part, "Compute") == 0)
                    bit = VK_SHADER_STAGE_COMPUTE_BIT;
                else if (std::strcmp(part, "AllGraphics") == 0)
                    bit = VK_SHADER_STAGE_ALL_GRAPHICS;
                else if (std::strcmp(part, "VertexFragment") == 0)
                    bit = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
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

        bool ParseUBOSemantic(const char *token, UBODescriptorSemantic &out) noexcept
        {
            static const char *const names[] =
            {
                "ViewportInfo", "CameraInfo", "SkyInfo", "MaterialColorPalette"
            };
            for (uint32 i = 0; i < 4; ++i)
            {
                if (std::strcmp(token, names[i]) == 0)
                {
                    out = static_cast<UBODescriptorSemantic>(i);
                    return true;
                }
            }
            return false;
        }

        bool ParseDescriptorSemantic(const char *token, DescriptorSemantic &out) noexcept
        {
            static const char *const names[] =
            {
                "ViewportInfo", "CameraInfo", "SkyInfo",
                "MaterialTexture", "MaterialSampler"
            };
            static const DescriptorSemantic values[] =
            {
                DescriptorSemantic::ViewportInfo, DescriptorSemantic::CameraInfo,
                DescriptorSemantic::SkyInfo,
                DescriptorSemantic::MaterialTexture, DescriptorSemantic::MaterialSampler
            };
            for (uint32 i = 0; i < 5; ++i)
            {
                if (std::strcmp(token, names[i]) == 0)
                {
                    out = values[i];
                    return true;
                }
            }
            return false;
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

        bool ParseTextureSlot(const char *token, TextureSlot &out) noexcept
        {
            static const char *const names[] =
            {
                "BaseColor", "Normal", "Metallic", "Roughness", "Emissive",
                "Occlusion", "OpacityMask", "Height", "Custom0", "Custom1"
            };
            for (uint32 i = 0; i < 10; ++i)
            {
                if (std::strcmp(token, names[i]) == 0)
                {
                    out = static_cast<TextureSlot>(i);
                    return true;
                }
            }
            return false;
        }

        bool ParseConditionDomain(
            const char *token,
            GLSLCodeModuleConditionDomain &out_domain) noexcept
        {
            if (std::strcmp(token, "Option") == 0)
                out_domain = GLSLCodeModuleConditionDomain::Option;
            else if (std::strcmp(token, "ShaderProgramPurpose") == 0)
                out_domain =
                    GLSLCodeModuleConditionDomain::ShaderProgramPurpose;
            else if (std::strcmp(token, "DeviceFeature") == 0)
                out_domain = GLSLCodeModuleConditionDomain::DeviceFeature;
            else
                return false;

            return true;
        }

        bool ParseConditionOperator(
            const char *token,
            GLSLCodeModuleConditionOperator &out_operation) noexcept
        {
            if (std::strcmp(token, "Equals") == 0)
                out_operation = GLSLCodeModuleConditionOperator::Equals;
            else if (std::strcmp(token, "NotEquals") == 0)
                out_operation = GLSLCodeModuleConditionOperator::NotEquals;
            else
                return false;

            return true;
        }
    }

    const char *GetGLSLCodeModuleParseResultName(const GLSLCodeModuleParseResult result) noexcept
    {
        switch (result)
        {
            case GLSLCodeModuleParseResult::Skipped: return "Skipped";
            case GLSLCodeModuleParseResult::OK: return "OK";
            case GLSLCodeModuleParseResult::MissingBegin: return "MissingBegin";
            case GLSLCodeModuleParseResult::DuplicateBegin: return "DuplicateBegin";
            case GLSLCodeModuleParseResult::MissingEnd: return "MissingEnd";
            case GLSLCodeModuleParseResult::UnknownDirective: return "UnknownDirective";
            case GLSLCodeModuleParseResult::DuplicateDirective: return "DuplicateDirective";
            case GLSLCodeModuleParseResult::MissingDirectiveArgument: return "MissingDirectiveArgument";
            case GLSLCodeModuleParseResult::InvalidKind: return "InvalidKind";
            case GLSLCodeModuleParseResult::InvalidSemantic: return "InvalidSemantic";
            case GLSLCodeModuleParseResult::InvalidSource: return "InvalidSource";
            case GLSLCodeModuleParseResult::InvalidNumericClass: return "InvalidNumericClass";
            case GLSLCodeModuleParseResult::InvalidNumber: return "InvalidNumber";
            case GLSLCodeModuleParseResult::InvalidResource: return "InvalidResource";
            case GLSLCodeModuleParseResult::InvalidStage: return "InvalidStage";
            case GLSLCodeModuleParseResult::MissingMetadataVersion: return "MissingMetadataVersion";
            case GLSLCodeModuleParseResult::UnsupportedMetadataVersion: return "UnsupportedMetadataVersion";
            case GLSLCodeModuleParseResult::InvalidCondition: return "InvalidCondition";
            case GLSLCodeModuleParseResult::InvalidDependency: return "InvalidDependency";
            case GLSLCodeModuleParseResult::InvalidConflict: return "InvalidConflict";
            default: return "Unknown";
        }
    }

    GLSLCodeModuleParseResult ParseGLSLCodeModuleFile(const char *content,
                                                      const int content_size,
                                                      GLSLCodeModuleFileData &out_data) noexcept
    {
        if (!content || content_size <= 0)
            return GLSLCodeModuleParseResult::Skipped;

        const char *end = content + content_size;
        const char *cursor = content;

        bool in_block = false;
        bool saw_any_ulre = false;
        bool saw_end = false;
        bool saw_version = false;
        bool saw_kind = false;
        bool saw_priority = false;
        bool saw_flags = false;
        bool requires_versioned_metadata = false;
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
                        return GLSLCodeModuleParseResult::MissingBegin;
                    return GLSLCodeModuleParseResult::UnknownDirective;
                }

                if (std::strcmp(token, "begin") == 0)
                {
                    if (in_block)
                        return GLSLCodeModuleParseResult::DuplicateBegin;
                    in_block = true;
                }
                else if (std::strcmp(token, "end") == 0)
                {
                    if (!in_block)
                        return GLSLCodeModuleParseResult::MissingBegin;
                    in_block = false;
                    saw_end = true;
                }
                else if (!in_block)
                {
                    return GLSLCodeModuleParseResult::MissingBegin;
                }
                else if (std::strcmp(token, "name") == 0)
                {
                    if (!out_data.name.IsEmpty())
                        return GLSLCodeModuleParseResult::DuplicateDirective;

                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GLSLCodeModuleParseResult::MissingDirectiveArgument;
                    out_data.name = AnsiString(token);
                }
                else if (std::strcmp(token, "version") == 0)
                {
                    if (saw_version)
                        return GLSLCodeModuleParseResult::DuplicateDirective;

                    const char *next =
                        ReadToken(after_keyword, line_end, token, sizeof(token));
                    uint32 version = 0;
                    if (!next || !ParseUnsignedInt(token, version))
                        return GLSLCodeModuleParseResult::InvalidNumber;
                    if (version > GLSLCodeModuleCurrentMetadataVersion)
                        return GLSLCodeModuleParseResult::
                            UnsupportedMetadataVersion;

                    out_data.metadata_version = static_cast<uint16>(version);
                    saw_version = true;
                }
                else if (std::strcmp(token, "kind") == 0)
                {
                    if (saw_kind)
                        return GLSLCodeModuleParseResult::DuplicateDirective;

                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GLSLCodeModuleParseResult::MissingDirectiveArgument;

                    GLSLCodeModuleKind kind;
                    if (!ParseKind(token, kind))
                        return GLSLCodeModuleParseResult::InvalidKind;
                    out_data.kind = kind;
                    saw_kind = true;
                }
                else if (std::strcmp(token, "priority") == 0)
                {
                    if (saw_priority)
                        return GLSLCodeModuleParseResult::DuplicateDirective;

                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GLSLCodeModuleParseResult::MissingDirectiveArgument;

                    int value = 0;
                    if (!ParseSignedInt(token, value))
                        return GLSLCodeModuleParseResult::InvalidNumber;
                    out_data.priority = value;
                    saw_priority = true;
                }
                else if (std::strcmp(token, "flags") == 0)
                {
                    if (saw_flags)
                        return GLSLCodeModuleParseResult::DuplicateDirective;

                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GLSLCodeModuleParseResult::MissingDirectiveArgument;

                    uint32 value = 0;
                    if (!ParseUnsignedInt(token, value))
                        return GLSLCodeModuleParseResult::InvalidNumber;
                    out_data.flags = value;
                    saw_flags = true;
                }
                else if (std::strcmp(token, "ubo") == 0)
                {
                    GLSLCodeModuleUBORequirement requirement;
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !ParseUBOSemantic(token, requirement.semantic))
                        return GLSLCodeModuleParseResult::InvalidResource;
                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !ParseStageFlags(token, requirement.stage_flags))
                        return GLSLCodeModuleParseResult::InvalidStage;

                    while ((next = ReadToken(next, line_end, token, sizeof(token))) != nullptr)
                    {
                        if (!ParseResourcePolicy(token, requirement.required,
                                                  requirement.allow_fallback))
                            return GLSLCodeModuleParseResult::InvalidResource;
                    }
                    out_data.ubo_requirements.Add(requirement);
                }
                else if (std::strcmp(token, "ssbo") == 0)
                {
                    GLSLCodeModuleSSBORequirement requirement;
                    const int name_index = out_data.ssbo_name_storage.GetCount();
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GLSLCodeModuleParseResult::MissingDirectiveArgument;
                    AnsiString *ssbo_name = out_data.ssbo_name_storage.Create();
                    if (!ssbo_name)
                        return GLSLCodeModuleParseResult::InvalidResource;
                    *ssbo_name = token;
                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !ParseSSBOType(token, requirement.ssbo_type))
                        return GLSLCodeModuleParseResult::InvalidResource;
                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !ParseUnsignedInt(token, requirement.data_slot))
                        return GLSLCodeModuleParseResult::InvalidNumber;
                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !ParseStageFlags(token, requirement.stage_flags))
                        return GLSLCodeModuleParseResult::InvalidStage;

                    while ((next = ReadToken(next, line_end, token, sizeof(token))) != nullptr)
                    {
                        if (!ParseResourcePolicy(token, requirement.required,
                                                  requirement.allow_fallback))
                            return GLSLCodeModuleParseResult::InvalidResource;
                    }
                    (void)name_index;
                    out_data.ssbo_requirements.Add(requirement);
                }
                else if (std::strcmp(token, "texture_layer") == 0)
                {
                    GLSLCodeModuleTextureLayerRequirement requirement;
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !ParseTextureSlot(token, requirement.slot))
                        return GLSLCodeModuleParseResult::InvalidResource;
                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !ParseStageFlags(token, requirement.stage_flags))
                        return GLSLCodeModuleParseResult::InvalidStage;

                    while ((next = ReadToken(next, line_end, token, sizeof(token))) != nullptr)
                    {
                        if (!ParseResourcePolicy(token, requirement.required,
                                                  requirement.allow_fallback))
                            return GLSLCodeModuleParseResult::InvalidResource;
                    }
                    out_data.texture_layer_requirements.Add(requirement);
                }
                else if (std::strcmp(token, "require") == 0)
                {
                    GLSLCodeModuleSemanticRequirement requirement;

                    // <source> <semantic> [<numclass> [<min> [<max>]]]
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GLSLCodeModuleParseResult::MissingDirectiveArgument;
                    if (!ParseSource(token, requirement.source))
                        return GLSLCodeModuleParseResult::InvalidSource;

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GLSLCodeModuleParseResult::MissingDirectiveArgument;
                    requirement.semantic = ParseSemantic(token);
                    if (requirement.semantic == GLSLCodeModuleSemantic::Unknown)
                        return GLSLCodeModuleParseResult::InvalidSemantic;

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (next && token[0])
                    {
                        uint32 mask = 0;
                        if (!ParseNumericClassMask(token, mask))
                            return GLSLCodeModuleParseResult::InvalidNumericClass;
                        requirement.numeric_class_mask = mask;

                        next = ReadToken(next, line_end, token, sizeof(token));
                        if (next && token[0])
                        {
                            uint32 min_components = 0;
                            if (!ParseUnsignedInt(token, min_components))
                                return GLSLCodeModuleParseResult::InvalidNumber;
                            requirement.min_component_count = uint8(min_components);

                            next = ReadToken(next, line_end, token, sizeof(token));
                            if (next && token[0])
                            {
                                uint32 max_components = 0;
                                if (!ParseUnsignedInt(token, max_components))
                                    return GLSLCodeModuleParseResult::InvalidNumber;
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
                        return GLSLCodeModuleParseResult::MissingDirectiveArgument;

                    const GLSLCodeModuleSemantic semantic = ParseSemantic(token);
                    if (semantic == GLSLCodeModuleSemantic::Unknown)
                        return GLSLCodeModuleParseResult::InvalidSemantic;
                    out_data.semantic_provides.Add(semantic);
                }
                else if (std::strcmp(token, "uses") == 0)
                {
                    const char *next = ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GLSLCodeModuleParseResult::MissingDirectiveArgument;

                    const AnsiString dependency_name(token);
                    GLSLCodeModuleDependency dependency{};

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (next && token[0])
                    {
                        uint32 min_version = 0;
                        if (!ParseUnsignedInt(token, min_version)
                         || min_version > GLSLCodeModuleCurrentMetadataVersion)
                            return GLSLCodeModuleParseResult::InvalidDependency;

                        next = ReadToken(next, line_end, token, sizeof(token));
                        uint32 max_version = 0;
                        if (!next
                         || !ParseUnsignedInt(token, max_version)
                         || max_version > GLSLCodeModuleCurrentMetadataVersion
                         || min_version > max_version)
                            return GLSLCodeModuleParseResult::InvalidDependency;

                        dependency.min_metadata_version =
                            static_cast<uint16>(min_version);
                        dependency.max_metadata_version =
                            static_cast<uint16>(max_version);
                        requires_versioned_metadata = true;
                    }

                    out_data.pending_module_requirements.Add(dependency_name);
                    out_data.pending_dependency_versions.Add(dependency);
                }
                else if (std::strcmp(token, "condition") == 0)
                {
                    GLSLCodeModuleCondition condition{};
                    const char *next =
                        ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !ParseConditionDomain(token, condition.domain))
                        return GLSLCodeModuleParseResult::InvalidCondition;

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GLSLCodeModuleParseResult::InvalidCondition;
                    AnsiString *key = out_data.condition_key_storage.Create();
                    if (!key)
                        return GLSLCodeModuleParseResult::InvalidCondition;
                    *key = token;
                    condition.key = key->c_str();

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next
                     || !ParseConditionOperator(token, condition.operation))
                        return GLSLCodeModuleParseResult::InvalidCondition;

                    next = ReadToken(next, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GLSLCodeModuleParseResult::InvalidCondition;
                    AnsiString *value =
                        out_data.condition_value_storage.Create();
                    if (!value)
                        return GLSLCodeModuleParseResult::InvalidCondition;
                    *value = token;
                    condition.value = value->c_str();

                    out_data.conditions.Add(condition);
                    requires_versioned_metadata = true;
                }
                else if (std::strcmp(token, "conflicts") == 0)
                {
                    const char *next =
                        ReadToken(after_keyword, line_end, token, sizeof(token));
                    if (!next || !token[0])
                        return GLSLCodeModuleParseResult::InvalidConflict;

                    out_data.pending_module_conflicts.Add(AnsiString(token));
                    requires_versioned_metadata = true;
                }
                else
                {
                    return GLSLCodeModuleParseResult::UnknownDirective;
                }
            }

            cursor = line_break ? line_break + 1 : end;
        }

        if (!saw_any_ulre)
            return GLSLCodeModuleParseResult::Skipped;

        if (in_block)
            return GLSLCodeModuleParseResult::MissingEnd;

        if (requires_versioned_metadata
         && out_data.metadata_version
                == GLSLCodeModuleUnversionedMetadataVersion)
            return GLSLCodeModuleParseResult::MissingMetadataVersion;

        return GLSLCodeModuleParseResult::OK;
    }
}
