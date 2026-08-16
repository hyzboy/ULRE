#include <hgl/mtl/MaterialDefinitionFile.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/type/Smart.h>
#include <toml/toml.hpp>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        bool ReadString(const toml::value &table, const char *key, AnsiString &out)
        {
            if (!table.is_table() || !table.contains(key) || !table.at(key).is_string())
                return false;
            const std::string &value = table.at(key).as_string();
            out = value.c_str();
            return true;
        }

        bool ReadString(const toml::value &table, const char *key, std::string &out)
        {
            if (!table.is_table() || !table.contains(key) || !table.at(key).is_string())
                return false;
            out = table.at(key).as_string();
            return true;
        }

        bool ReadBool(const toml::value &table, const char *key, bool &out)
        {
            if (!table.is_table() || !table.contains(key) || !table.at(key).is_boolean())
                return false;
            out = table.at(key).as_boolean();
            return true;
        }

        bool ReadFloat(const toml::value &table, const char *key, float &out)
        {
            if (!table.is_table() || !table.contains(key))
                return false;
            const toml::value &value = table.at(key);
            if (value.is_floating())
                out = static_cast<float>(value.as_floating());
            else if (value.is_integer())
                out = static_cast<float>(value.as_integer());
            else
                return false;
            return true;
        }

        bool ReadRequiredString(const toml::value &table, const char *key, std::string &out)
        {
            return ReadString(table, key, out) && !out.empty();
        }

        bool ParseUsage(const std::string &name, MaterialDefinitionUsageTag &out)
        {
            if (name == "General") out = MaterialDefinitionUsageTag::General;
            else if (name == "Fallback") out = MaterialDefinitionUsageTag::Fallback;
            else if (name == "Debug") out = MaterialDefinitionUsageTag::Debug;
            else if (name == "Text") out = MaterialDefinitionUsageTag::Text;
            else if (name == "Sky") out = MaterialDefinitionUsageTag::Sky;
            else return false;
            return true;
        }

        bool ParseBootstrap(const std::string &name, MaterialDefinitionBootstrapKind &out)
        {
            if (name == "None") out = MaterialDefinitionBootstrapKind::None;
            else if (name == "ErrorCheckerboard") out = MaterialDefinitionBootstrapKind::ErrorCheckerboard;
            else if (name == "PureColor") out = MaterialDefinitionBootstrapKind::PureColor;
            else if (name == "PureDepth") out = MaterialDefinitionBootstrapKind::PureDepth;
            else if (name == "TextAlphaBlend") out = MaterialDefinitionBootstrapKind::TextAlphaBlend;
            else return false;
            return true;
        }

        bool ParsePolicy(const std::string &name, MaterialVertexProviderPolicy &out)
        {
            if (name == "Auto") out = MaterialVertexProviderPolicy::Auto;
            else if (name == "GeometryOnly") out = MaterialVertexProviderPolicy::GeometryOnly;
            else if (name == "AllowDerived") out = MaterialVertexProviderPolicy::AllowDerived;
            else return false;
            return true;
        }

        // Accepts all registered SurfaceType names for forward compatibility.
        // Skin/Hair/Cloth/Eye/Foliage/ClearCoat/Water are reserved — they resolve to
        // lit_surface at compositor assembly time.
        bool ParseSurface(const std::string &name, SurfaceType &out)
        {
            static const char *const names[] = {
                "Unlit", "Lit", "Skin", "Hair", "Cloth",
                "Eye", "Foliage", "ClearCoat", "Water", "Sky"
            };
            for (uint32 i = 0; i < 10; ++i)
            {
                if (name == names[i])
                {
                    out = static_cast<SurfaceType>(i);
                    return true;
                }
            }
            return false;
        }

        bool ParseBlend(const std::string &name, BlendMode &out)
        {
            if (name == "Opaque") out = BlendMode::Opaque;
            else if (name == "Masked") out = BlendMode::Masked;
            else if (name == "Transparent") out = BlendMode::Transparent;
            else if (name == "Dither") out = BlendMode::Dither;
            else if (name == "AlphaToCoverage") out = BlendMode::AlphaToCoverage;
            else return false;
            return true;
        }

        bool ParsePass(const std::string &name, PassType &out)
        {
            static const char *const names[] = {
                "ForwardOpaque", "ForwardMasked", "ForwardTransparent",
                "ForwardDither", "ForwardA2C", "ShadowOpaque", "ShadowMasked",
                "EarlyZSolid", "EarlyZMasked"
            };
            for (uint32 i = 0; i < 9; ++i)
            {
                if (name == names[i])
                {
                    out = static_cast<PassType>(i);
                    return true;
                }
            }
            return false;
        }

        bool ParseCullMode(const std::string &name, VkCullModeFlags &out)
        {
            if (name == "None") out = VK_CULL_MODE_NONE;
            else if (name == "Front") out = VK_CULL_MODE_FRONT_BIT;
            else if (name == "Back") out = VK_CULL_MODE_BACK_BIT;
            else if (name == "FrontAndBack") out = VK_CULL_MODE_FRONT_AND_BACK;
            else return false;
            return true;
        }

        bool ParseCompareOp(const std::string &name, VkCompareOp &out)
        {
            static const VkCompareOp values[] = {
                VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL,
                VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER,
                VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL,
                VK_COMPARE_OP_ALWAYS
            };
            static const char *const names[] = {
                "Never", "Less", "Equal", "LessOrEqual", "Greater",
                "NotEqual", "GreaterOrEqual", "Always"
            };
            for (uint32 i = 0; i < 8; ++i)
            {
                if (name == names[i])
                {
                    out = values[i];
                    return true;
                }
            }
            return false;
        }

        bool ParseBlendFactor(const std::string &name, VkBlendFactor &out)
        {
            static const VkBlendFactor values[] = {
                VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE,
                VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
                VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
                VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
                VK_BLEND_FACTOR_SRC_ALPHA_SATURATE
            };
            static const char *const names[] = {
                "Zero", "One", "SrcAlpha", "OneMinusSrcAlpha",
                "DstAlpha", "OneMinusDstAlpha", "SrcColor",
                "OneMinusSrcColor", "DstColor", "OneMinusDstColor",
                "SrcAlphaSaturate"
            };
            for (uint32 i = 0; i < 11; ++i)
            {
                if (name == names[i])
                {
                    out = values[i];
                    return true;
                }
            }
            return false;
        }

        bool ParseRenderState(const toml::value &root, MaterialDefinition &definition)
        {
            if (!root.contains("render_state"))
                return true;

            const toml::value &render_state = root.at("render_state");
            if (!render_state.is_table())
                return false;

            if (render_state.contains("double_sided")
             && !ReadBool(render_state, "double_sided",
                          definition.default_render_state.double_sided))
                return false;
            if (render_state.contains("alpha_test")
             && !ReadBool(render_state, "alpha_test",
                          definition.default_render_state.alpha_test))
                return false;
            if (render_state.contains("alpha_cutoff")
             && !ReadFloat(render_state, "alpha_cutoff",
                           definition.default_render_state.alpha_cutoff))
                return false;
            if (render_state.contains("dither")
             && !ReadBool(render_state, "dither",
                          definition.default_render_state.dither))
                return false;

            if (!render_state.contains("pipeline"))
                return true;
            const toml::value &pipeline = render_state.at("pipeline");
            if (!pipeline.is_table())
                return false;

            std::string value;
            if (pipeline.contains("cull_mode"))
            {
                if (!ReadString(pipeline, "cull_mode", value)
                 || !ParseCullMode(value, definition.default_render_state.pipeline_config.cull_mode))
                    return false;
            }
            if (pipeline.contains("depth_compare_op"))
            {
                if (!ReadString(pipeline, "depth_compare_op", value)
                 || !ParseCompareOp(value, definition.default_render_state.pipeline_config.depth_compare_op))
                    return false;
            }
            if (pipeline.contains("blend_src"))
            {
                if (!ReadString(pipeline, "blend_src", value)
                 || !ParseBlendFactor(value, definition.default_render_state.pipeline_config.blend_src))
                    return false;
            }
            if (pipeline.contains("blend_dst"))
            {
                if (!ReadString(pipeline, "blend_dst", value)
                 || !ParseBlendFactor(value, definition.default_render_state.pipeline_config.blend_dst))
                    return false;
            }

            bool &depth_test = definition.default_render_state.pipeline_config.depth_test;
            bool &depth_write = definition.default_render_state.pipeline_config.depth_write;
            bool &alpha_blend = definition.default_render_state.pipeline_config.alpha_blend;
            bool &alpha_to_coverage =
                definition.default_render_state.pipeline_config.alpha_to_coverage;
            bool &dynamic_line_width =
                definition.default_render_state.pipeline_config.dynamic_line_width;
            bool &overlay = definition.default_render_state.pipeline_config.overlay;
            bool &wireframe = definition.default_render_state.pipeline_config.wireframe;
            if (pipeline.contains("depth_test") && !ReadBool(pipeline, "depth_test", depth_test))
                return false;
            if (pipeline.contains("depth_write") && !ReadBool(pipeline, "depth_write", depth_write))
                return false;
            if (pipeline.contains("alpha_blend") && !ReadBool(pipeline, "alpha_blend", alpha_blend))
                return false;
            if (pipeline.contains("alpha_to_coverage")
             && !ReadBool(pipeline, "alpha_to_coverage", alpha_to_coverage))
                return false;
            if (pipeline.contains("dynamic_line_width")
             && !ReadBool(pipeline, "dynamic_line_width", dynamic_line_width))
                return false;
            if (pipeline.contains("overlay") && !ReadBool(pipeline, "overlay", overlay))
                return false;
            if (pipeline.contains("wireframe") && !ReadBool(pipeline, "wireframe", wireframe))
                return false;
            if (pipeline.contains("line_width")
             && !ReadFloat(pipeline, "line_width",
                           definition.default_render_state.pipeline_config.line_width))
                return false;
            return true;
        }

        bool ParseVertexInput(const std::string &name, VertexInputMode &out)
        {
            if (name == "Vec2Position") out = VertexInputMode::Vec2Position;
            else if (name == "Vec3Position") out = VertexInputMode::Vec3Position;
            else if (name == "Vec2IntPosition") out = VertexInputMode::Vec2IntPosition;
            else if (name == "Procedural") out = VertexInputMode::Procedural;
            else return false;
            return true;
        }

        bool ParseMapping(const std::string &name, PositionMappingMode &out)
        {
            static const char *const names[] = {
                "Passthrough3D", "LiftXY_XY0", "LiftXY_X0Y", "LiftXY_0XY",
                "NDCLift", "ZeroOneToNDC", "PixelToLocal", "TerrainGrid"
            };
            for (uint32 i = 0; i < 8; ++i)
            {
                if (name == names[i])
                {
                    out = static_cast<PositionMappingMode>(i);
                    return true;
                }
            }
            return false;
        }

        bool ParseOrientation(const std::string &name, OrientationMode &out)
        {
            if (name == "World") out = OrientationMode::World;
            else if (name == "CameraFacingFree") out = OrientationMode::CameraFacingFree;
            else if (name == "CameraFacingAxisY") out = OrientationMode::CameraFacingAxisY;
            else return false;
            return true;
        }

        bool ParseScale(const std::string &name, ScaleMode &out)
        {
            if (name == "World") out = ScaleMode::World;
            else if (name == "FixedPixelSize") out = ScaleMode::FixedPixelSize;
            else return false;
            return true;
        }

        bool ParseProjection(const std::string &name, ProjectionMode &out)
        {
            if (name == "WorldCameraVP") out = ProjectionMode::WorldCameraVP;
            else if (name == "LocalToWorldOnly") out = ProjectionMode::LocalToWorldOnly;
            else if (name == "OrthoViewport") out = ProjectionMode::OrthoViewport;
            else if (name == "OrthoThenLocalToWorld") out = ProjectionMode::OrthoThenLocalToWorld;
            else if (name == "ClipPassthrough") out = ProjectionMode::ClipPassthrough;
            else return false;
            return true;
        }

        bool ParseTransformGraph(const toml::value &table,
                                 VertexShaderNodeConfig &out)
        {
            std::string value;
            if (!ReadRequiredString(table, "source", value)
             || !ParseVertexInput(value, out.input)
             || !ReadRequiredString(table, "mapping", value)
             || !ParseMapping(value, out.position_mapping)
             || !ReadRequiredString(table, "orientation", value)
             || !ParseOrientation(value, out.orientation)
             || !ReadRequiredString(table, "scale", value)
             || !ParseScale(value, out.scale)
             || !ReadRequiredString(table, "projection", value)
             || !ParseProjection(value, out.projection))
                return false;
            return true;
        }

        bool ParseSemantic(const std::string &name, GLSLCodeModuleSemantic &out)
        {
            static const char *const names[] = {
                "Position", "UV0", "Color", "ColorY", "ColorUV",
                "Normal", "Tangent", "Binormal", "WorldPosition",
                "WorldNormal", "WorldTangent", "WorldBinormal", "Luminance",
                "HeightMap", "Camera", "Viewport", "SkyLight",
                "MaterialData", "TransformID"
            };
            static const GLSLCodeModuleSemantic values[] = {
                GLSLCodeModuleSemantic::Position, GLSLCodeModuleSemantic::UV0,
                GLSLCodeModuleSemantic::Color, GLSLCodeModuleSemantic::ColorY,
                GLSLCodeModuleSemantic::ColorUV, GLSLCodeModuleSemantic::Normal,
                GLSLCodeModuleSemantic::Tangent, GLSLCodeModuleSemantic::Binormal,
                GLSLCodeModuleSemantic::WorldPosition, GLSLCodeModuleSemantic::WorldNormal,
                GLSLCodeModuleSemantic::WorldTangent, GLSLCodeModuleSemantic::WorldBinormal,
                GLSLCodeModuleSemantic::Luminance, GLSLCodeModuleSemantic::HeightMap,
                GLSLCodeModuleSemantic::Camera, GLSLCodeModuleSemantic::Viewport,
                GLSLCodeModuleSemantic::SkyLight, GLSLCodeModuleSemantic::MaterialData,
                GLSLCodeModuleSemantic::TransformID
            };
            for (uint32 i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
            {
                if (name == names[i])
                {
                    out = values[i];
                    return true;
                }
            }
            return false;
        }

        bool ParseSSBOType(const std::string &name, SSBOType &out)
        {
            for (uint32 i = 0; i < static_cast<uint32>(SSBOType::RANGE_SIZE); ++i)
            {
                const SSBOType type = static_cast<SSBOType>(i);
                if (name == GetSSBOTypeName(type))
                {
                    out = type;
                    return true;
                }
            }
            return false;
        }

        bool ParseSampler(const std::string &name, GLSLSamplerType &out)
        {
            static const char *const names[] = {
                "Sampler2D", "Sampler2DArray", "Sampler2DShadow",
                "SamplerCube", "SamplerCubeArray", "Sampler3D"
            };
            for (uint32 i = 0; i < 6; ++i)
            {
                if (name == names[i])
                {
                    out = static_cast<GLSLSamplerType>(i);
                    return true;
                }
            }
            return false;
        }

        bool ParseDefinition(const toml::value &root, MaterialDefinitionFileData &out)
        {
            std::string id;
            std::string name;
            std::string source;
            std::string usage;
            std::string bootstrap;
            std::string policy;

            if (!ReadRequiredString(root, "id", id)
             || !ReadRequiredString(root, "name", name)
             || !ReadRequiredString(root, "source", source)
             || !ReadRequiredString(root, "usage", usage)
             || !ReadRequiredString(root, "bootstrap", bootstrap)
             || !ReadRequiredString(root, "provider_policy", policy)
             || source != "file"
             || !ParseUsage(usage, out.definition.usage_tag)
             || !ParseBootstrap(bootstrap, out.definition.bootstrap_kind)
             || !ParsePolicy(policy, out.definition.vertex_provider_policy))
                return false;

            out.definition.definition_id = id;
            out.definition.definition_name = name;
            out.definition.source_kind = MaterialDefinitionSourceKind::File;
            if (root.contains("transform"))
            {
                if (!ParseTransformGraph(
                        root.at("transform"), out.definition.vertex_node_config))
                    return false;
            }

            const toml::value *fragment = root.contains("fragment")
                ? &root.at("fragment") : nullptr;
            const toml::value *compositor = root.contains("compositor")
                ? &root.at("compositor") : nullptr;
            std::string value;
            if (fragment)
            {
                if (!ReadRequiredString(*fragment, "source", value))
                    return false;
                out.fragment_source_storage = value.c_str();
                SetMaterialFragmentSource(
                    out.definition, out.fragment_source_storage.c_str());
                if (fragment->contains("surface_module"))
                {
                    if (!ReadRequiredString(*fragment, "surface_module", value))
                        return false;
                    out.surface_module_storage = value.c_str();
                    out.definition.fragment_surface_module =
                        out.surface_module_storage.c_str();
                }
                if (fragment->contains("material_source_module"))
                {
                    if (!ReadRequiredString(*fragment, "material_source_module", value))
                        return false;
                    out.material_source_module_storage = value.c_str();
                    out.definition.fragment_material_source_module =
                        out.material_source_module_storage.c_str();
                }
                if (fragment->contains("ntb_module"))
                {
                    if (!ReadRequiredString(*fragment, "ntb_module", value))
                        return false;
                    out.ntb_module_storage = value.c_str();
                    out.definition.fragment_ntb_module =
                        out.ntb_module_storage.c_str();
                }
            }
            else
               return false;

            if (!compositor
              || !ReadRequiredString(*compositor, "surface", value)
              || !ParseSurface(value, out.definition.compositor_surface)
              || !ReadRequiredString(*compositor, "blend", value)
              || !ParseBlend(value, out.definition.compositor_blend)
              || !ReadRequiredString(*compositor, "pass", value)
              || !ParsePass(value, out.definition.compositor_pass))
               return false;

            if (!out.definition.fragment_source)
                return false;

            if (!ParseRenderState(root, out.definition))
                return false;

            const toml::value *vertex = root.contains("vertex") ? &root.at("vertex") : nullptr;
            if (!vertex || !vertex->contains("requirements")
             || !vertex->at("requirements").is_array())
                return false;
            for (const auto &item : vertex->at("requirements").as_array())
            {
                if (!item.is_string())
                    return false;
                GLSLCodeModuleSemantic semantic;
                if (!ParseSemantic(item.as_string(), semantic))
                    return false;
                GLSLCodeModuleSemanticRequirement requirement;
                requirement.source = GLSLCodeModuleCapabilitySource::ProducedSemantic;
                requirement.semantic = semantic;
                out.definition.vertex_semantic_requirements.Add(requirement);
            }

            if (root.contains("resources"))
            {
                const toml::value &resources = root.at("resources");
                if (resources.contains("ubos"))
                {
                    if (!resources.at("ubos").is_array())
                        return false;
                    static const char *const names[] = {
                        "ViewportInfo", "CameraInfo", "SkyInfo", "MaterialColorPalette"
                    };
                    for (const auto &item : resources.at("ubos").as_array())
                    {
                        if (!item.is_string())
                            return false;
                        bool found = false;
                        for (uint32 i = 0; i < 4; ++i)
                        {
                            if (item.as_string() == names[i])
                            {
                                out.definition.ubo_requirements.push_back(
                                    static_cast<UBODescriptorSemantic>(i));
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                            return false;
                    }
                }

                if (resources.contains("code_modules"))
                {
                    if (!resources.at("code_modules").is_array())
                        return false;
                    for (const auto &item : resources.at("code_modules").as_array())
                    {
                        if (!item.is_string())
                            return false;
                        // 模块名即身份（注册表唯一键），字符串直存。
                        out.definition.code_module_requirements.emplace_back(
                            item.as_string().c_str());
                    }
                }

                if (resources.contains("ssbos"))
                {
                    if (!resources.at("ssbos").is_array())
                        return false;
                    for (const auto &item : resources.at("ssbos").as_array())
                    {
                        if (!item.is_table()
                         || !item.contains("name") || !item.at("name").is_string()
                         || !item.contains("type") || !item.at("type").is_string())
                            return false;
                        const std::string slot_name = item.at("name").as_string();
                        if (!IsValidMaterialDataSlotName(slot_name)
                         || out.definition.data_slot_decls.size() >= MaxMaterialDataSlotsPerMaterial)
                            return false;
                        for (const auto &existing : out.definition.data_slot_decls)
                        {
                            if (existing.name == slot_name)
                                return false;
                        }
                        SSBOType type;
                        if (!ParseSSBOType(item.at("type").as_string(), type))
                            return false;
                        out.definition.data_slot_decls.push_back(
                            {slot_name, type});
                    }
                }

                if (resources.contains("textures"))
                {
                    if (!resources.at("textures").is_array())
                        return false;
                    for (const auto &item : resources.at("textures").as_array())
                    {
                        if (!item.is_table()
                         || !item.contains("name") || !item.at("name").is_string()
                         || !item.contains("sampler") || !item.at("sampler").is_string()
                         || !item.contains("required") || !item.at("required").is_boolean())
                            return false;
                        const std::string slot_name = item.at("name").as_string();
                        TextureSlot slot;
                        GLSLSamplerType sampler;
                        if (!ParseTextureSlotName(slot_name, slot)
                         || !ParseSampler(item.at("sampler").as_string(), sampler))
                            return false;
                        out.definition.texture_slot_decls.push_back(
                            {slot_name, slot, sampler, item.at("required").as_boolean()});
                    }
                }

                if (resources.contains("samplers"))
                {
                    if (!resources.at("samplers").is_array())
                        return false;
                    for (const auto &item : resources.at("samplers").as_array())
                    {
                        if (!item.is_string())
                            return false;
                        const std::string name = item.as_string();
                        if (name.empty())
                            return false;
                        for (const auto &existing : out.definition.sampler_names)
                        {
                            if (existing == name)
                                return false;
                        }
                        out.definition.sampler_names.push_back(name);
                    }
                }
            }

            if (root.contains("vertex") && root.at("vertex").contains("varyings"))
            {
                const toml::value &varyings = root.at("vertex").at("varyings");
                if (!varyings.is_array())
                    return false;
                for (const auto &item : varyings.as_array())
                {
                    if (!item.is_string())
                        return false;
                    const std::string field = item.as_string();
                    if (field == "emit_data_index_id") out.definition.vertex_varying.emit_data_index_id = true;
                    else if (field == "emit_vertex_color") out.definition.vertex_varying.emit_vertex_color = true;
                    else if (field == "emit_uv0") out.definition.vertex_varying.emit_uv0 = true;
                    else if (field == "emit_world_pos") out.definition.vertex_varying.emit_world_pos = true;
                    else if (field == "emit_world_normal") out.definition.vertex_varying.emit_world_normal = true;
                    else if (field == "emit_luminance") out.definition.vertex_varying.emit_luminance = true;
                    else if (field == "emit_frag_direction") out.definition.vertex_varying.emit_frag_direction = true;
                    else if (field == "use_transform_id_attr") out.definition.vertex_varying.use_transform_id_attr = true;
                    else if (field == "emit_vertex_color_from_palette") out.definition.vertex_varying.emit_vertex_color_from_palette = true;
                    else return false;
                }
            }

            return IsValidMaterialDefinitionFileData(out);
        }
    }

    const char *GetMaterialDefinitionFileParseResultName(
        const MaterialDefinitionFileParseResult result) noexcept
    {
        switch (result)
        {
        case MaterialDefinitionFileParseResult::Skipped: return "Skipped";
        case MaterialDefinitionFileParseResult::OK: return "OK";
        case MaterialDefinitionFileParseResult::InvalidValue: return "InvalidValue";
        default: return "Invalid";
        }
    }

    MaterialDefinitionFileParseResult ParseMaterialDefinitionFile(
        const char *content,
        const int content_size,
        MaterialDefinitionFileData &out_data) noexcept
    {
        out_data = MaterialDefinitionFileData{};
        if (!content || content_size <= 0)
            return MaterialDefinitionFileParseResult::Skipped;

        try
        {
            const toml::value root = toml::parse_str(
                std::string(content, static_cast<size_t>(content_size)));
            if (!root.is_table() || !root.contains("schema")
             || !root.at("schema").is_integer()
             || root.at("schema").as_integer() != 1)
                return MaterialDefinitionFileParseResult::InvalidValue;
            return ParseDefinition(root, out_data)
                ? MaterialDefinitionFileParseResult::OK
                : MaterialDefinitionFileParseResult::InvalidValue;
        }
        catch (const toml::exception &)
        {
            return MaterialDefinitionFileParseResult::InvalidValue;
        }
    }

    bool IsValidMaterialDefinitionFileData(
        const MaterialDefinitionFileData &data) noexcept
    {
        const MaterialDefinition &definition = data.definition;
        return !definition.definition_id.empty()
            && !definition.definition_name.empty()
            && definition.source_kind == MaterialDefinitionSourceKind::File
            && !definition.vertex_semantic_requirements.IsEmpty()
            && definition.fragment_source
            && definition.fragment_source[0] != 0;
    }

    bool MaterialDefinitionFileRegistry::LoadFile(const OSString &path)
    {
        hgl::io::OpenFileInputStream opener(path);
        if (!opener)
            return false;
        const int64 size = opener->GetSize();
        if (size <= 0)
            return false;

        hgl::AutoDeleteArray<char> buffer(size_t(size) + 1);
        if (!buffer || opener->Read(buffer.data(), size) != size)
            return false;
        buffer[size_t(size)] = 0;

        MaterialDefinitionFileData *data = files.Create();
        if (!data)
            return false;

        if (ParseMaterialDefinitionFile(buffer.data(), static_cast<int>(size), *data)
            != MaterialDefinitionFileParseResult::OK)
        {
            files.DeleteAt(files.GetCount() - 1);
            return false;
        }

        const int new_file_index = files.GetCount() - 1;
        for (int i = 0; i < new_file_index; ++i)
        {
            if (files[i]
             && files[i]->definition.definition_id
                    == data->definition.definition_id)
            {
                files.DeleteAt(new_file_index);
                return false;
            }
        }
        return true;
    }

    bool MaterialDefinitionFileRegistry::LoadDirectory(
        const OSString &directory, int *out_file_count, int *out_error_count)
    {
        hgl::ValueArray<hgl::filesystem::FileInfo> file_list;
        const int scan_count = hgl::filesystem::GetFileInfoList(
            file_list, directory, true, true, true);
        if (scan_count < 0)
            return false;

        int file_count = 0;
        int error_count = 0;
        for (int i = 0; i < file_list.GetCount(); ++i)
        {
            const auto &file = file_list[i];
            if (!file.is_file)
                continue;
            const OSString name(file.name);
            if (!name.EndsWith(OS_TEXT(".material.toml")))
                continue;
            if (LoadFile(OSString(file.fullname)))
                ++file_count;
            else
                ++error_count;
        }

        if (out_file_count) *out_file_count = file_count;
        if (out_error_count) *out_error_count = error_count;
        return true;
    }

    const MaterialDefinition *MaterialDefinitionFileRegistry::FindByID(
        const char *definition_id) const
    {
        if (!definition_id || !definition_id[0])
            return nullptr;
        for (int i = 0; i < files.GetCount(); ++i)
        {
            if (files[i] && files[i]->definition.definition_id == definition_id)
                return &files[i]->definition;
        }
        return nullptr;
    }
}
