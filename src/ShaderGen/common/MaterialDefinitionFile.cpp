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

        bool ParseMode(const std::string &name, MaterialFragmentProgramMode &out)
        {
            if (name == "DirectInclude") out = MaterialFragmentProgramMode::DirectInclude;
            else if (name == "Compositor") out = MaterialFragmentProgramMode::Compositor;
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

        bool ParseSurface(const std::string &name, SurfaceType &out)
        {
            static const char *const names[] = {
                "Unlit", "Standard", "Skin", "Hair", "Cloth",
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
                "EarlyZSolid", "EarlyZMasked", "VBufferID"
            };
            for (uint32 i = 0; i < 10; ++i)
            {
                if (name == names[i])
                {
                    out = static_cast<PassType>(i);
                    return true;
                }
            }
            return false;
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
                                 MaterialTransformGraph &out)
        {
            std::string value;
            if (!ReadRequiredString(table, "source", value)
             || !ParseVertexInput(value, out.source)
             || !ReadRequiredString(table, "mapping", value)
             || !ParseMapping(value, out.mapping)
             || !ReadRequiredString(table, "orientation", value)
             || !ParseOrientation(value, out.orientation)
             || !ReadRequiredString(table, "scale", value)
             || !ParseScale(value, out.scale)
             || !ReadRequiredString(table, "projection", value)
             || !ParseProjection(value, out.projection))
                return false;
            return true;
        }

        bool ParseCodeModule(const std::string &name, GLSLCodeModuleID &out)
        {
            static const char *const names[] = {
                "SkyLightHeader", "SkyLightSimple", "SkyLightCubeMap",
                "PBRSurface"
            };
            for (uint32 i = 0; i < 4; ++i)
            {
                if (name == names[i])
                {
                    out = static_cast<GLSLCodeModuleID>(i);
                    return true;
                }
            }
            return false;
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

        bool ParseTextureSlot(const std::string &name, TextureSlot &out)
        {
            static const char *const names[] = {
                "BaseColor", "Normal", "Metallic", "Roughness", "Emissive",
                "Occlusion", "OpacityMask", "Height", "Custom0", "Custom1"
            };
            for (uint32 i = 0; i < 10; ++i)
            {
                if (name == names[i])
                {
                    out = static_cast<TextureSlot>(i);
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
            std::string mode;
            std::string policy;

            if (!ReadRequiredString(root, "id", id)
             || !ReadRequiredString(root, "name", name)
             || !ReadRequiredString(root, "source", source)
             || !ReadRequiredString(root, "usage", usage)
             || !ReadRequiredString(root, "bootstrap", bootstrap)
             || !ReadRequiredString(root, "program_mode", mode)
             || !ReadRequiredString(root, "provider_policy", policy)
             || source != "file"
             || !ParseUsage(usage, out.definition.usage_tag)
             || !ParseBootstrap(bootstrap, out.definition.bootstrap_kind)
             || !ParseMode(mode, out.definition.fragment_program_mode)
             || !ParsePolicy(policy, out.definition.vertex_provider_policy))
                return false;

            out.definition.definition_id = id;
            out.definition.definition_name = name;
            out.definition.source_kind = MaterialDefinitionSourceKind::File;
            if (root.contains("transform"))
            {
                if (!ParseTransformGraph(
                        root.at("transform"), out.definition.transform_graph))
                    return false;
                out.definition.has_transform_graph = true;
                out.definition.vertex_node_config =
                    out.definition.transform_graph.ToNodeConfig();
            }

            const toml::value *compositor = nullptr;
            if (root.contains("compositor"))
            {
                compositor = &root.at("compositor");
                std::string value;
                if (!ReadRequiredString(*compositor, "fragment", value))
                    return false;
                out.fragment_module_storage = value.c_str();
                out.definition.fragment_program_module = out.fragment_module_storage.c_str();
                if (out.definition.fragment_program_mode == MaterialFragmentProgramMode::Compositor
                 && (!ReadRequiredString(*compositor, "surface", value)
                  || !ParseSurface(value, out.definition.compositor_surface)
                  || !ReadRequiredString(*compositor, "blend", value)
                  || !ParseBlend(value, out.definition.compositor_blend)
                  || !ReadRequiredString(*compositor, "pass", value)
                  || !ParsePass(value, out.definition.compositor_pass)))
                    return false;
                if (compositor->contains("surface_module"))
                {
                    if (!ReadRequiredString(*compositor, "surface_module", value))
                        return false;
                    out.surface_module_storage = value.c_str();
                    out.definition.fragment_surface_module = out.surface_module_storage.c_str();
                }
            }
            if (!out.definition.fragment_program_module)
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
                        GLSLCodeModuleID module;
                        if (!ParseCodeModule(item.as_string(), module))
                            return false;
                        out.definition.code_module_requirements.push_back(module);
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
                        SSBOType type;
                        if (!ParseSSBOType(item.at("type").as_string(), type))
                            return false;
                        out.definition.ssbo_slot_decls.push_back(
                            {item.at("name").as_string(), type});
                    }
                }

                if (resources.contains("textures"))
                {
                    if (!resources.at("textures").is_array())
                        return false;
                    for (const auto &item : resources.at("textures").as_array())
                    {
                        if (!item.is_table()
                         || !item.contains("slot") || !item.at("slot").is_string()
                         || !item.contains("sampler") || !item.at("sampler").is_string()
                         || !item.contains("required") || !item.at("required").is_boolean())
                            return false;
                        TextureSlot slot;
                        GLSLSamplerType sampler;
                        if (!ParseTextureSlot(item.at("slot").as_string(), slot)
                         || !ParseSampler(item.at("sampler").as_string(), sampler))
                            return false;
                        out.definition.texture_slot_decls.push_back(
                            {slot, sampler, item.at("required").as_boolean(), nullptr});
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
                    else if (field == "emit_texture_layer_id") out.definition.vertex_varying.emit_texture_layer_id = true;
                    else if (field == "texture_layer_id_uses_data_index") out.definition.vertex_varying.texture_layer_id_uses_data_index = true;
                    else if (field == "emit_vertex_color") out.definition.vertex_varying.emit_vertex_color = true;
                    else if (field == "emit_uv0") out.definition.vertex_varying.emit_uv0 = true;
                    else if (field == "emit_world_pos") out.definition.vertex_varying.emit_world_pos = true;
                    else if (field == "emit_world_normal") out.definition.vertex_varying.emit_world_normal = true;
                    else if (field == "emit_luminance") out.definition.vertex_varying.emit_luminance = true;
                    else if (field == "emit_frag_direction") out.definition.vertex_varying.emit_frag_direction = true;
                    else if (field == "use_transform_id_attr") out.definition.vertex_varying.use_transform_id_attr = true;
                    else if (field == "emit_vertex_color_from_pattle") out.definition.vertex_varying.emit_vertex_color_from_pattle = true;
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
            && definition.fragment_program_module
            && definition.fragment_program_module[0] != 0;
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
