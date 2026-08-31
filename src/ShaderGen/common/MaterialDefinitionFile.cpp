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

        bool ParseBootstrap(const std::string &name, MaterialDefinitionBootstrapKind &out)
        {
            static const struct { const char *name; MaterialDefinitionBootstrapKind value; } table[] = {
                { "None",          MaterialDefinitionBootstrapKind::None },
                { "PureColor",     MaterialDefinitionBootstrapKind::PureColor },
                { "TextAlphaBlend", MaterialDefinitionBootstrapKind::TextAlphaBlend },
            };
            for (const auto &entry : table)
            {
                if (name == entry.name)
                {
                    out = entry.value;
                    return true;
                }
            }
            return false;
        }

        bool ParsePolicy(const std::string &name, MaterialVertexProviderPolicy &out)
        {
            static const struct { const char *name; MaterialVertexProviderPolicy value; } table[] = {
                { "Auto",         MaterialVertexProviderPolicy::Auto },
                { "GeometryOnly", MaterialVertexProviderPolicy::GeometryOnly },
                { "AllowDerived", MaterialVertexProviderPolicy::AllowDerived },
            };
            for (const auto &entry : table)
            {
                if (name == entry.name)
                {
                    out = entry.value;
                    return true;
                }
            }
            return false;
        }

        bool ParseSurface(const std::string &name, SurfaceType &out)
        {
            static const char *const names[] = {
                "Unlit", "Lit", "Sky"
            };
            for (uint32 i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
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
            static const struct { const char *name; BlendMode value; } table[] = {
                { "Opaque",        BlendMode::Opaque },
                { "Masked",        BlendMode::Masked },
                { "Transparent",   BlendMode::Transparent },
                { "Dither",        BlendMode::Dither },
                { "AlphaToCoverage", BlendMode::AlphaToCoverage },
            };
            for (const auto &entry : table)
            {
                if (name == entry.name)
                {
                    out = entry.value;
                    return true;
                }
            }
            return false;
        }

        bool ParsePass(const std::string &name, PassType &out)
        {
            // T1：PassType 名字单一真源 PassType.h（GetPassTypeName/ParsePassType）
            return ParsePassType(name.c_str(), out);
        }

        bool ParseCullMode(const std::string &name, VkCullModeFlags &out)
        {
            static const struct { const char *name; VkCullModeFlags value; } table[] = {
                { "None",         VK_CULL_MODE_NONE },
                { "Front",        VK_CULL_MODE_FRONT_BIT },
                { "Back",         VK_CULL_MODE_BACK_BIT },
                { "FrontAndBack", VK_CULL_MODE_FRONT_AND_BACK },
            };
            for (const auto &entry : table)
            {
                if (name == entry.name)
                {
                    out = entry.value;
                    return true;
                }
            }
            return false;
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
            static const struct { const char *name; VertexInputMode value; } table[] = {
                { "Vec2Position",    VertexInputMode::Vec2Position },
                { "Vec3Position",    VertexInputMode::Vec3Position },
                { "Vec2IntPosition", VertexInputMode::Vec2IntPosition },
                { "None",            VertexInputMode::None },
            };
            for (const auto &entry : table)
            {
                if (name == entry.name)
                {
                    out = entry.value;
                    return true;
                }
            }
            return false;
        }

        bool ParseMapping(const std::string &name, PositionMappingMode &out)
        {
            static const char *const names[] = {
                "Passthrough3D", "LiftXY_XY0", "LiftXY_X0Y", "LiftXY_0XY",
                "NDCLift", "ZeroOneToNDC", "PixelToLocal"
            };
            for (uint32 i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
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
            static const struct { const char *name; OrientationMode value; } table[] = {
                { "World",            OrientationMode::World },
                { "CameraFacingFree", OrientationMode::CameraFacingFree },
                { "CameraFacingAxisY", OrientationMode::CameraFacingAxisY },
            };
            for (const auto &entry : table)
            {
                if (name == entry.name)
                {
                    out = entry.value;
                    return true;
                }
            }
            return false;
        }

        bool ParseScale(const std::string &name, ScaleMode &out)
        {
            static const struct { const char *name; ScaleMode value; } table[] = {
                { "World",          ScaleMode::World },
                { "FixedPixelSize", ScaleMode::FixedPixelSize },
            };
            for (const auto &entry : table)
            {
                if (name == entry.name)
                {
                    out = entry.value;
                    return true;
                }
            }
            return false;
        }

        bool ParseProjection(const std::string &name, ProjectionMode &out)
        {
            static const struct { const char *name; ProjectionMode value; } table[] = {
                { "WorldCameraVP",        ProjectionMode::WorldCameraVP },
                { "LocalToWorldOnly",     ProjectionMode::LocalToWorldOnly },
                { "OrthoViewport",        ProjectionMode::OrthoViewport },
                { "OrthoThenLocalToWorld", ProjectionMode::OrthoThenLocalToWorld },
                { "ClipPassthrough",      ProjectionMode::ClipPassthrough },
            };
            for (const auto &entry : table)
            {
                if (name == entry.name)
                {
                    out = entry.value;
                    return true;
                }
            }
            return false;
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
            // T1：语义名字单一真源 GLSLCodeModule.h（GetGLSLCodeModuleSemanticName/
            // ParseGLSLCodeModuleSemantic）——此处不再维护平行表。
            return ParseGLSLCodeModuleSemantic(name.c_str(), out);
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
            std::string bootstrap;
            std::string policy;

            if (!ReadRequiredString(root, "id", id)
             || !ReadRequiredString(root, "name", name)
             || !ReadRequiredString(root, "source", source)
             || !ReadRequiredString(root, "bootstrap", bootstrap)
             || !ReadRequiredString(root, "provider_policy", policy)
             || source != "file"
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
                    static const DescriptorSemantic semantic_values[] = {
                        DescriptorSemantic::ViewportInfo,
                        DescriptorSemantic::CameraInfo,
                        DescriptorSemantic::SkyInfo,
                        DescriptorSemantic::MaterialColorPalette
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
                                    semantic_values[i]);
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

                if (resources.contains("material_data"))
                {
                    // 单槽化：一个材质只有一个私有数据 SSBO（MaterialPrivateData）。
                    // TOML 形态：material_data = { type = "EmissiveSurface" }
                    // 无 name 段（固定 DefaultMaterialPrivateDataSlotName）；type 必须是材质 SSBO 类型。
                    const auto &item = resources.at("material_data");
                    if (!item.is_table()
                     || !item.contains("type") || !item.at("type").is_string())
                        return false;
                    SSBOType type;
                    if (!ParseSSBOType(item.at("type").as_string(), type)
                     || type == SSBOType::UserDefined
                     || !IsMaterialSSBOType(type))
                        return false;
                    out.definition.material_private_data_slot_decls.push_back(
                        {DefaultMaterialPrivateDataSlotName, type});
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

                if (resources.contains("defines"))
                {
                    if (!resources.at("defines").is_array())
                        return false;
                    for (const auto &item : resources.at("defines").as_array())
                    {
                        if (!item.is_string())
                            return false;
                        const std::string name = item.as_string();
                        if (name.empty())
                            return false;
                        for (const auto &existing : out.definition.compile_defines)
                        {
                            if (existing == name)
                                return false;
                        }
                        out.definition.compile_defines.push_back(name);
                    }
                }
            }

            if (root.contains("vertex") && root.at("vertex").contains("varyings"))
            {
                const toml::value &varyings = root.at("vertex").at("varyings");
                if (!varyings.is_array())
                    return false;
                // T2：varying 字段表驱动（成员指针表——字段名单一真源在此表，
                // 新增 varying 只需加一行）
                struct VaryingFieldEntry
                {
                    const char *name;
                    bool MaterialVertexVaryingConfig::*field;
                };
                static const VaryingFieldEntry kVaryingFieldTable[] = {
                    { "emit_data_index_id",             &MaterialVertexVaryingConfig::emit_data_index_id },
                    { "emit_vertex_color",              &MaterialVertexVaryingConfig::emit_vertex_color },
                    { "emit_uv0",                       &MaterialVertexVaryingConfig::emit_uv0 },
                    { "emit_world_pos",                 &MaterialVertexVaryingConfig::emit_world_pos },
                    { "emit_world_normal",              &MaterialVertexVaryingConfig::emit_world_normal },
                    { "emit_luminance",                 &MaterialVertexVaryingConfig::emit_luminance },
                    { "emit_frag_direction",            &MaterialVertexVaryingConfig::emit_frag_direction },
                    { "use_transform_id_attr",          &MaterialVertexVaryingConfig::use_transform_id_attr },
                    { "emit_vertex_color_from_palette", &MaterialVertexVaryingConfig::emit_vertex_color_from_palette },
                    { "emit_style_id",                  &MaterialVertexVaryingConfig::emit_style_id },
                };
                for (const auto &item : varyings.as_array())
                {
                    if (!item.is_string())
                        return false;
                    const std::string field = item.as_string();
                    bool found = false;
                    for (const VaryingFieldEntry &entry : kVaryingFieldTable)
                    {
                        if (field == entry.name)
                        {
                            out.definition.vertex_varying.*(entry.field) = true;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        return false;
                }
            }

            // ── [mesh_shader] 段（可选）──────────────────────────────
            if (root.contains("mesh_shader"))
            {
                const toml::value &ms = root.at("mesh_shader");
                if (ms.contains("mode"))
                {
                    std::string mode_str;
                    if (!ReadRequiredString(ms, "mode", mode_str))
                        return false;
                    if (!ParseMeshShaderMode(mode_str.c_str(), out.definition.mesh_shader_mode))
                        return false;   // 未知模式名 = 拼写错误，硬失败而非静默降级
                }
                if (ms.contains("max_invocations"))
                {
                    const auto &val = ms.at("max_invocations");
                    if (!val.is_integer())
                        return false;
                    out.definition.mesh_shader_max_invocations =
                        static_cast<uint32_t>(val.as_integer());
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
        case MaterialDefinitionFileParseResult::UnknownKey: return "UnknownKey";
        default: return "Invalid";
        }
    }

    // 层内键白名单校验：解析器不认识的键 → false。未知键 = 作者笔误或已删字段
    // 残留（如 usage），静默吞掉会让作者以为配置生效而实际被忽略——TOML 化
    // 材质最危险的失效模式，故显式报错。
    static bool ValidateKnownKeys(
        const toml::value &table,
        std::initializer_list<const char *> allowed) noexcept
    {
        if (!table.is_table())
            return true;
        const auto &tbl = table.as_table();
        for (const auto &pair : tbl)
        {
            const std::string &key = pair.first;
            bool known = false;
            for (const char *name : allowed)
            {
                if (key == name)
                {
                    known = true;
                    break;
                }
            }
            if (!known)
                return false;
        }
        return true;
    }

    // 顶层 + 各子层键白名单（与 ParseDefinition/ParseRenderState 支持的键逐项对应）
    static bool ValidateMaterialDefinitionKeys(const toml::value &root) noexcept
    {
        if (!ValidateKnownKeys(root, {
                "schema", "id", "name", "source", "bootstrap", "provider_policy",
                "transform", "fragment", "compositor", "vertex", "resources",
                "mesh_shader", "render_state"}))
            return false;

        if (root.contains("transform")
         && !ValidateKnownKeys(root.at("transform"), {
                "source", "mapping", "orientation", "projection", "scale"}))
            return false;

        if (root.contains("fragment")
         && !ValidateKnownKeys(root.at("fragment"), {
                "source", "surface_module", "material_source_module", "ntb_module"}))
            return false;

        if (root.contains("compositor")
         && !ValidateKnownKeys(root.at("compositor"), {
                "surface", "blend", "pass"}))
            return false;

        if (root.contains("vertex")
         && !ValidateKnownKeys(root.at("vertex"), {
                "requirements", "varyings"}))
            return false;

        if (root.contains("resources")
         && !ValidateKnownKeys(root.at("resources"), {
                "ubos", "material_data", "samplers", "textures", "defines"}))
            return false;

        if (root.contains("mesh_shader")
         && !ValidateKnownKeys(root.at("mesh_shader"), {
                "mode", "max_invocations"}))
            return false;

        if (root.contains("render_state"))
        {
            const toml::value &rs = root.at("render_state");
            if (!ValidateKnownKeys(rs, {
                    "double_sided", "alpha_test", "alpha_cutoff", "dither", "pipeline"}))
                return false;
            if (rs.contains("pipeline")
             && !ValidateKnownKeys(rs.at("pipeline"), {
                    "cull_mode", "depth_compare_op", "blend_src", "blend_dst",
                    "depth_test", "depth_write", "alpha_blend", "alpha_to_coverage",
                    "dynamic_line_width", "overlay", "wireframe", "line_width"}))
                return false;
        }

        return true;
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
            if (!ParseDefinition(root, out_data))
                return MaterialDefinitionFileParseResult::InvalidValue;
            if (!ValidateMaterialDefinitionKeys(root))
                return MaterialDefinitionFileParseResult::UnknownKey;
            return MaterialDefinitionFileParseResult::OK;
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
