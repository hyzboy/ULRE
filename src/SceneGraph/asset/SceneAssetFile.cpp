#include <hgl/graph/asset/SceneAssetFile.h>
#include <hgl/asset/generated/SceneFlat.gen.h>
#include <hgl/asset/generated/SceneToml.gen.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/log/Log.h>
#include <hgl/type/Smart.h>
#include <flatbuffers/flatbuffers.h>
#include <toml/toml.hpp>

#include <cmath>
#include <cstring>

DEFINE_LOGGER_MODULE(SceneAsset)
USE_MODULE_LOGGER(SceneAsset)

namespace asset_codec = ::hgl::asset::generated;

namespace hgl::graph
{
    namespace
    {
        bool ValidateRelationships(const toml::value &root,
                                   const SceneSpec &spec,
                                   bool validate_material_key_shape = true);
    }

    bool LoadSceneSpecFromFile(const OSString &filename,
                               SceneSpec &out_spec)
    {
        if (filename.EndsWithIgnoreCase(OS_TEXT(".toml")))
            return LoadSceneSpecFromTOML(filename, out_spec);
        if (filename.EndsWithIgnoreCase(OS_TEXT(".scene.fb")))
            return LoadSceneSpecFromFlatBuffers(filename, out_spec);

        MLogError(SceneAsset, OS_TEXT("LoadSceneSpecFromFile: unsupported scene file extension in ")
                  + filename);
        return false;
    }

    bool LoadSceneSpecFromTOML(const OSString &filename,
                               SceneSpec &out_spec)
    {
        hgl::io::OpenFileInputStream opener(filename);
        if (!opener)
        {
            MLogError(SceneAsset, OS_TEXT("LoadSceneSpecFromTOML: cannot open file ")
                      + filename);
            return false;
        }

        const int64 size = opener->GetSize();
        if (size <= 0 || size > 16 * 1024 * 1024)
        {
            MLogError(SceneAsset, OS_TEXT("LoadSceneSpecFromTOML: invalid file size in ")
                      + filename);
            return false;
        }

        hgl::AutoDeleteArray<char> buffer(static_cast<size_t>(size) + 1);
        if (!buffer || opener->Read(buffer.data(), size) != size)
        {
            MLogError(SceneAsset, OS_TEXT("LoadSceneSpecFromTOML: read failed in ")
                      + filename);
            return false;
        }

        buffer[static_cast<size_t>(size)] = '\0';
        return ParseSceneSpec(buffer.data(), static_cast<size_t>(size),
                              filename.c_str(), out_spec);
    }

    bool LoadSceneSpecFromFlatBuffers(const OSString &filename,
                                      SceneSpec &out_spec)
    {
        hgl::io::OpenFileInputStream opener(filename);
        if (!opener)
        {
            MLogError(SceneAsset, OS_TEXT("LoadSceneSpecFromFlatBuffers: cannot open file ")
                      + filename);
            return false;
        }

        const int64 size = opener->GetSize();
        if (size <= 0 || size > 64 * 1024 * 1024)
        {
            MLogError(SceneAsset, OS_TEXT("LoadSceneSpecFromFlatBuffers: invalid file size in ")
                      + filename);
            return false;
        }

        hgl::AutoDeleteArray<uint8> buffer(static_cast<size_t>(size));
        if (!buffer || opener->Read(buffer.data(), size) != size)
        {
            MLogError(SceneAsset, OS_TEXT("LoadSceneSpecFromFlatBuffers: read failed in ")
                      + filename);
            return false;
        }

        return ParseSceneSpecFromFlatBuffers(
            buffer.data(), static_cast<size_t>(size), filename.c_str(), out_spec);
    }

    bool SaveSceneSpecToFlatBuffers(const SceneSpec &spec,
                                    const OSString &filename)
    {
        if (spec.schema != 1 || spec.name.Length() == 0)
        {
            MLogError(SceneAsset, OS_TEXT("SaveSceneSpecToFlatBuffers: invalid scene specification"));
            return false;
        }

        const toml::value validation_table = asset_codec::WriteSceneSpec(spec);
        if (!ValidateRelationships(validation_table, spec, false))
        {
            MLogError(SceneAsset, OS_TEXT("SaveSceneSpecToFlatBuffers: invalid scene relationships"));
            return false;
        }

        flatbuffers::FlatBufferBuilder builder;
        const auto root = asset_codec::EncodeSceneSpec(builder, spec);
        builder.Finish(root);

        hgl::AutoDelete<hgl::io::FileOutputStream> output(
            hgl::io::CreateFileOutputStream(filename));
        if (!output)
        {
            MLogError(SceneAsset, OS_TEXT("SaveSceneSpecToFlatBuffers: cannot create file ")
                      + filename);
            return false;
        }

        if (output->Write(builder.GetBufferPointer(), builder.GetSize())
            != static_cast<int64>(builder.GetSize()))
        {
            MLogError(SceneAsset, OS_TEXT("SaveSceneSpecToFlatBuffers: write failed in ")
                      + filename);
            return false;
        }

        return true;
    }

    bool ParseSceneSpecFromFlatBuffers(const void *data,
                                       const size_t size,
                                       const os_char *debug_name,
                                       SceneSpec &out_spec)
    {
        if (!data || size == 0 || size > 64 * 1024 * 1024)
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpecFromFlatBuffers: invalid buffer in ")
                      + OSString(debug_name));
            return false;
        }

        flatbuffers::Verifier verifier(
            static_cast<const uint8_t *>(data), size);
        if (!verifier.VerifyBuffer<hgl::asset::schema::SceneSpec>(nullptr))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpecFromFlatBuffers: verification failed in ")
                      + OSString(debug_name));
            return false;
        }

        out_spec.schema = 1;
        out_spec.name = "";
        out_spec.description = "";
        out_spec.camera = SceneCameraSpec{};
        out_spec.clear_color = SceneClearColorSpec{};
        out_spec.materials.Clear();
        out_spec.nodes.Clear();
        const auto *root =
            flatbuffers::GetRoot<hgl::asset::schema::SceneSpec>(data);
        asset_codec::DecodeSceneSpec(root, out_spec);
        out_spec.camera.enabled = root->camera() && root->camera()->enabled();
        out_spec.clear_color.enabled =
            root->clear_color() && root->clear_color()->enabled();

        if (out_spec.schema != 1 || out_spec.name.Length() == 0)
            return false;

        const toml::value validation_table = asset_codec::WriteSceneSpec(out_spec);
        if (!ValidateRelationships(validation_table, out_spec, false))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpecFromFlatBuffers: semantic validation failed in ")
                      + OSString(debug_name));
            return false;
        }

        return true;
    }

    namespace
    {
        bool IsAllowedKey(const char *key, const char *const *allowed, const size_t count)
        {
            for (size_t i = 0; i < count; ++i)
                if (std::strcmp(key, allowed[i]) == 0)
                    return true;
            return false;
        }

        bool HasOnlyKeys(const toml::value &value,
                         const char *const *allowed,
                         const size_t count)
        {
            if (!value.is_table())
                return false;

            for (const auto &entry : value.as_table())
                if (!IsAllowedKey(entry.first.c_str(), allowed, count))
                    return false;

            return true;
        }

        bool IsFinite(const Color4f &value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y)
                && std::isfinite(value.z) && std::isfinite(value.w);
        }

        bool IsFinite(const math::Vector3f &value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y)
                && std::isfinite(value.z);
        }

        bool HasField(const toml::value &table, const char *field)
        {
            return table.is_table() && table.contains(field);
        }

        bool ValidateMaterialKeys(const toml::value &table,
                                  const SceneMaterialType type)
        {
            static const char *const pbr[] = {
                "name", "type", "base_color", "metallic", "roughness",
                "normal_scale", "fresnel", "textures"
            };
            static const char *const emissive[] = {"name", "type", "color", "textures"};
            static const char *const transmission[] = {"name", "type", "trans_color", "textures"};

            switch (type)
            {
            case SceneMaterialType::PBRSurface:
                return HasOnlyKeys(table, pbr, sizeof(pbr) / sizeof(pbr[0]));
            case SceneMaterialType::EmissiveSurface:
                return HasOnlyKeys(table, emissive, sizeof(emissive) / sizeof(emissive[0]));
            case SceneMaterialType::TransmissionSurface:
                return HasOnlyKeys(table, transmission, sizeof(transmission) / sizeof(transmission[0]));
            default:
                return false;
            }
        }

        bool IsValidMaterialType(const SceneMaterialType type)
        {
            switch (type)
            {
            case SceneMaterialType::PBRSurface:
            case SceneMaterialType::EmissiveSurface:
            case SceneMaterialType::TransmissionSurface:
                return true;
            default:
                return false;
            }
        }

        bool IsValidCameraMode(const SceneCameraMode mode)
        {
            switch (mode)
            {
            case SceneCameraMode::ViewModel:
            case SceneCameraMode::Free:
            case SceneCameraMode::LookAt:
            case SceneCameraMode::FirstPerson:
                return true;
            default:
                return false;
            }
        }

        bool NormalizeTextureTables(toml::value &root)
        {
            if (!root.contains("materials") || !root.at("materials").is_array())
                return true;

            for (auto &material : root.as_table()["materials"].as_array())
            {
                if (!material.is_table() || !material.contains("textures"))
                    continue;

                toml::value &textures = material.as_table()["textures"];
                if (!textures.is_table())
                    continue;

                toml::array normalized;
                for (const auto &entry : textures.as_table())
                {
                    if (!entry.second.is_string())
                        return false;

                    toml::table texture;
                    texture["name"] = entry.first;
                    texture["uri"] = entry.second;
                    normalized.push_back(toml::value(texture));
                }
                textures = toml::value(normalized);
            }

            return true;
        }

        bool ValidateRelationships(const toml::value &root,
                                   const SceneSpec &spec,
                                   const bool validate_material_key_shape)
        {
            if (spec.camera.enabled
                && (!IsValidCameraMode(spec.camera.mode)
                    || !IsFinite(spec.camera.position)
                    || !IsFinite(spec.camera.target)
                    || !std::isfinite(spec.camera.distance)
                    || !std::isfinite(spec.camera.yaw)
                    || !std::isfinite(spec.camera.pitch)
                    || !std::isfinite(spec.camera.fov_deg)
                    || !std::isfinite(spec.camera.near_plane)
                    || !std::isfinite(spec.camera.far_plane)
                    || spec.camera.fov_deg <= 0.0f
                    || spec.camera.fov_deg >= 180.0f
                    || spec.camera.pitch <= -90.0f
                    || spec.camera.pitch >= 90.0f
                    || spec.camera.near_plane <= 0.0f
                    || spec.camera.far_plane <= spec.camera.near_plane
                    || ((spec.camera.mode == SceneCameraMode::ViewModel
                         || spec.camera.mode == SceneCameraMode::LookAt)
                        && spec.camera.distance <= 0.0f)))
                return false;

            if (spec.clear_color.enabled && !IsFinite(spec.clear_color.value))
                return false;

            for (int i = 0; i < spec.materials.GetCount(); ++i)
            {
                const SceneMaterialSpec &material = *spec.materials[i];
                if (material.name.IsEmpty())
                    return false;

                for (int j = 0; j < i; ++j)
                    if (material.name == spec.materials[j]->name)
                        return false;

                const toml::value &source = root.at("materials").as_array()[size_t(i)];
                if (!HasField(source, "type")
                    || !IsValidMaterialType(material.type)
                    || (validate_material_key_shape
                        && !ValidateMaterialKeys(source, material.type))
                    || !IsFinite(material.base_color)
                    || !IsFinite(material.color)
                    || !IsFinite(material.trans_color))
                    return false;

                if (source.contains("textures"))
                {
                    const toml::value &textures = source.at("textures");
                    if (!textures.is_array())
                        return false;
                    const auto &texture_array = textures.as_array();
                    for (size_t texture_index = 0;
                         texture_index < texture_array.size();
                         ++texture_index)
                    {
                        const auto &texture = texture_array[texture_index];
                        static const char *const texture_keys[] = {"name", "uri", "required"};
                        if (!HasOnlyKeys(texture, texture_keys,
                                         sizeof(texture_keys) / sizeof(texture_keys[0])))
                            return false;

                        if (!texture.contains("name")
                            || !texture.at("name").is_string()
                            || texture.at("name").as_string().empty()
                            || !texture.contains("uri")
                            || !texture.at("uri").is_string()
                            || texture.at("uri").as_string().empty())
                            return false;

                        for (size_t previous_index = 0;
                             previous_index < texture_index;
                             ++previous_index)
                            if (texture_array[previous_index].at("name").as_string()
                                == texture.at("name").as_string())
                                return false;
                    }
                }
            }

            const auto &nodes = spec.nodes;
            for (int i = 0; i < nodes.GetCount(); ++i)
            {
                const SceneNodeSpec &node = *nodes[i];
                if (node.name.IsEmpty() || node.geometry.IsEmpty()
                    || std::strncmp(node.geometry.c_str(), "presets:", 8) != 0
                    || std::strlen(node.geometry.c_str()) == 8
                    || node.recipe.IsEmpty()
                    || (hgl::stricmp(node.mobility.c_str(), "static") != 0
                        && hgl::stricmp(node.mobility.c_str(), "movable") != 0)
                    || !IsFinite(node.transform.position)
                    || !IsFinite(node.transform.rotation_axis)
                    || !IsFinite(node.transform.scale)
                    || !std::isfinite(node.transform.rotation_angle_deg))
                    return false;

                const math::Vector3f &axis = node.transform.rotation_axis;
                if (node.transform.rotation_angle_deg != 0.0f
                    && axis.x * axis.x + axis.y * axis.y + axis.z * axis.z < 1e-12f)
                    return false;

                for (int j = 0; j < i; ++j)
                    if (node.name == nodes[j]->name)
                        return false;

                if (!node.material.IsEmpty())
                {
                    bool found = false;
                    for (int j = 0; j < spec.materials.GetCount(); ++j)
                        if (node.material == spec.materials[j]->name)
                        {
                            found = true;
                            break;
                        }
                    if (!found)
                        return false;
                }

                int parent_index = -1;
                if (!node.parent.IsEmpty())
                {
                    for (int j = 0; j < nodes.GetCount(); ++j)
                        if (node.parent == nodes[j]->name)
                        {
                            parent_index = j;
                            break;
                        }
                    if (parent_index < 0 || parent_index == i)
                        return false;
                }

                int current = parent_index;
                int steps = 0;
                while (current >= 0)
                {
                    if (++steps > nodes.GetCount())
                        return false;

                    const SceneNodeSpec &parent = *nodes[current];
                    current = -1;
                    if (!parent.parent.IsEmpty())
                    {
                        for (int j = 0; j < nodes.GetCount(); ++j)
                            if (parent.parent == nodes[j]->name)
                            {
                                current = j;
                                break;
                            }
                    }
                }
            }

            return true;
        }
    }

    bool ParseSceneSpec(const char *content,
                        const size_t size,
                        const os_char *debug_name,
                        SceneSpec &out_spec)
    {
        if (!content || size == 0)
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: empty content in ") + OSString(debug_name));
            return false;
        }

        if (size > 0x7FFFFFFFu || std::memchr(content, '\0', size))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: invalid content size or embedded null in ")
                      + OSString(debug_name));
            return false;
        }

        AnsiString terminated_content(content, static_cast<int>(size));
        toml::value root;
        try
        {
            root = toml::parse_str(terminated_content.c_str());
        }
        catch (const toml::exception &e)
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: TOML parse error in ")
                      + OSString(debug_name) + OS_TEXT(": ") + ToOSString(e.what()));
            return false;
        }

        static const char *const root_keys[] = {
            "schema", "name", "description", "camera", "clear_color",
            "materials", "nodes"
        };
        if (!HasOnlyKeys(root, root_keys, sizeof(root_keys) / sizeof(root_keys[0])))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: root must be a table with known keys in ")
                      + OSString(debug_name));
            return false;
        }

        out_spec.schema = 1;
        out_spec.name = "";
        out_spec.description = "";
        out_spec.camera = SceneCameraSpec{};
        out_spec.clear_color = SceneClearColorSpec{};
        out_spec.materials.Clear();
        out_spec.nodes.Clear();

        if (!NormalizeTextureTables(root)
            || !asset_codec::ReadSceneSpec(root, out_spec)
            || !root.contains("schema")
            || out_spec.schema != 1
            || out_spec.name.IsEmpty()
            || !root.contains("nodes")
            || !root.at("nodes").is_array())
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: invalid schema or required fields in ")
                      + OSString(debug_name));
            return false;
        }

        if (root.contains("camera"))
            out_spec.camera.enabled = true;
        if (root.contains("clear_color"))
            out_spec.clear_color.enabled = true;

        static const char *const material_keys[] = {
            "name", "type", "base_color", "metallic", "roughness",
            "normal_scale", "fresnel", "color", "trans_color", "textures"
        };
        static const char *const node_keys[] = {
            "name", "parent", "geometry", "recipe", "material",
            "mobility", "visible", "transform"
        };
        static const char *const transform_keys[] = {
            "position", "rotation_axis", "rotation_angle_deg", "scale"
        };
        static const char *const camera_keys[] = {
            "enabled", "mode", "position", "target", "distance", "yaw",
            "pitch", "fov_deg", "near_plane", "far_plane", "main"
        };
        static const char *const clear_color_keys[] = {"enabled", "value"};

        if ((root.contains("camera")
             && !HasOnlyKeys(root.at("camera"), camera_keys,
                             sizeof(camera_keys) / sizeof(camera_keys[0])))
            || (root.contains("clear_color")
                && !HasOnlyKeys(root.at("clear_color"), clear_color_keys,
                                sizeof(clear_color_keys) / sizeof(clear_color_keys[0]))))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: invalid camera or clear_color table in ")
                      + OSString(debug_name));
            return false;
        }

        if (root.contains("materials") && !root.at("materials").is_array())
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: materials must be an array in ")
                      + OSString(debug_name));
            return false;
        }

        if (root.contains("materials"))
        {
            const auto &materials = root.at("materials").as_array();
            for (size_t i = 0; i < materials.size(); ++i)
                if (!HasOnlyKeys(materials[i], material_keys, sizeof(material_keys) / sizeof(material_keys[0])))
                {
                    MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: invalid material table in ")
                              + OSString(debug_name));
                    return false;
                }
        }

        const auto &nodes = root.at("nodes").as_array();
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            if (!HasOnlyKeys(nodes[i], node_keys, sizeof(node_keys) / sizeof(node_keys[0])))
            {
                MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: invalid node table in ")
                          + OSString(debug_name));
                return false;
            }
            const auto &node = nodes[i];
            if (node.contains("transform")
                && !HasOnlyKeys(node.at("transform"), transform_keys,
                                sizeof(transform_keys) / sizeof(transform_keys[0])))
            {
                MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: invalid transform table in ")
                          + OSString(debug_name));
                return false;
            }
        }

        if (!ValidateRelationships(root, out_spec))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: invalid material/node reference, type, or transform in ")
                      + OSString(debug_name));
            return false;
        }

        return true;
    }
}
