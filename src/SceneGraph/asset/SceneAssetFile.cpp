#include <hgl/graph/asset/SceneAssetFile.h>
#include <hgl/asset/generated/SceneSerialize.gen.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/log/Log.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/serialize/MetaSchema.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/type/Smart.h>
#include <hgl/utf.h>

#include <cmath>
#include <cstring>

DEFINE_LOGGER_MODULE(SceneAsset)
USE_MODULE_LOGGER(SceneAsset)


namespace hgl::graph
{
    namespace
    {
        bool ValidateRelationships(const SceneSpec &spec);
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

        // 语义校验只看 `SceneSpec` 本身。旧实现先 `WriteTomlTable` 造一张假表再回头
        // 从表里取值做校验，等于让「能否落盘」依赖 TOML 编码器；现在这条依赖没有了。
        if (!ValidateRelationships(spec))
        {
            MLogError(SceneAsset, OS_TEXT("SaveSceneSpecToFlatBuffers: invalid scene relationships"));
            return false;
        }

        hgl::ValueArray<uint8> buffer;
        if (!hgl::serialize::fb::EncodeToFlatBuffers<SceneSpec>(buffer, spec))
        {
            MLogError(SceneAsset, OS_TEXT("SaveSceneSpecToFlatBuffers: serialization failed"));
            return false;
        }

        hgl::AutoDelete<hgl::io::FileOutputStream> output(
            hgl::io::CreateFileOutputStream(filename));
        if (!output)
        {
            MLogError(SceneAsset, OS_TEXT("SaveSceneSpecToFlatBuffers: cannot create file ")
                      + filename);
            return false;
        }

        if (output->Write(buffer.GetData(), buffer.GetCount())
            != static_cast<int64>(buffer.GetCount()))
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

        if (!hgl::serialize::fb::VerifyBuffer<SceneSpec>(data, size))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpecFromFlatBuffers: verification failed in ")
                      + OSString(debug_name));
            return false;
        }

        // 归零放在最前面：连续解码多份缓冲时，上一次的 `materials` / `nodes` 不能残留。
        out_spec = SceneSpec{};

        if (!hgl::serialize::fb::DecodeFromFlatBuffers<SceneSpec>(
                static_cast<const uint8_t *>(data), size, out_spec))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpecFromFlatBuffers: decode failed in ")
                      + OSString(debug_name));
            return false;
        }

        if (out_spec.schema != 1 || out_spec.name.Length() == 0)
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpecFromFlatBuffers: invalid schema or name in ")
                      + OSString(debug_name));
            return false;
        }

        if (!ValidateRelationships(out_spec))
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

        bool HasOnlyKeys(const hgl::serialize::Value &value,
                         const char *const *allowed,
                         const size_t count)
        {
            if (!value.IsTable())
                return false;

            const int field_count = value.GetFieldCount();
            for (int i = 0; i < field_count; ++i)
            {
                const char *key = value.GetFieldName(i);
                if (!key || !IsAllowedKey(key, allowed, count))
                    return false;
            }

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

        /// 按材质类型分派的**键形**校验。
        ///
        /// `ValidateKeysByMeta` 只能校验「这些键是不是 `SceneMaterialSpec` 的字段」，
        /// 而 `SceneMaterialSpec` 是三种材质的并集，所以它分不清「EmissiveSurface
        /// 写了 metallic」。这条按类型收窄的规则属于场景语义，留在这里。
        bool ValidateMaterialKeys(const hgl::serialize::Value &table,
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

        bool HasPrefix(const AnsiString &value, const char *prefix)
        {
            const char *text = value.c_str();
            return text && std::strncmp(text, prefix, std::strlen(prefix)) == 0;
        }

        bool IsValidGeometryReference(const SceneNodeSpec &node)
        {
            if (node.geometry.IsEmpty())
                return false;

            if (HasPrefix(node.geometry, "presets:")
                || HasPrefix(node.geometry, "geo:"))
            {
                return node.geometry.Length() > (HasPrefix(node.geometry, "geo:") ? 4 : 8)
                    && !node.recipe.IsEmpty();
            }

            if (HasPrefix(node.geometry, "primitive:"))
                return node.geometry.Length() > 10
                    && node.recipe.IsEmpty()
                    && node.material.IsEmpty();

            return false;
        }

        /// 把 `[materials.textures]` 的「槽位名 = uri」简写展开成 `[[materials.textures]]`
        /// 的 `{name, uri}` 数组。必须在键校验之前跑：简写形态下 `textures` 是表，键是
        /// 槽位名（`base_color`），`SceneTextureSpec` 里没有这些属性，键校验会直接判错。
        bool NormalizeTextureTables(hgl::serialize::Value &root)
        {
            hgl::serialize::Value *materials = root.FindFieldExact("materials");
            if (!materials || !materials->IsArray())
                return true;

            const int material_count = materials->GetElementCount();
            for (int i = 0; i < material_count; ++i)
            {
                hgl::serialize::Value *material = materials->GetElement(i);
                if (!material || !material->IsTable())
                    continue;

                hgl::serialize::Value *textures = material->FindFieldExact("textures");
                if (!textures || !textures->IsTable())
                    continue;

                const int slot_count = textures->GetFieldCount();
                for (int j = 0; j < slot_count; ++j)
                {
                    const hgl::serialize::Value *uri = textures->GetFieldValue(j);
                    if (!uri || !uri->IsString())
                        return false;
                }

                hgl::serialize::Value normalized;
                normalized.SetArray();

                for (int j = 0; j < slot_count; ++j)
                {
                    const char                *slot = textures->GetFieldName(j);
                    const hgl::serialize::Value *uri = textures->GetFieldValue(j);

                    if (!slot || !slot[0] || !uri)
                        return false;

                    hgl::serialize::Value *entry = normalized.AddElement();
                    if (!entry)
                        return false;

                    entry->SetTable();

                    hgl::serialize::Value *name_field = entry->AddField("name", 0);
                    hgl::serialize::Value *uri_field = entry->AddField("uri", 0);
                    if (!name_field || !uri_field)
                        return false;

                    name_field->SetString(slot);
                    uri_field->SetString(uri->GetString());
                }

                textures->CopyFrom(normalized);
            }

            return true;
        }

        /// 材质表的**按类型**键形校验。旧实现在 `ValidateRelationships` 里靠后端原生表
        /// 定位每张材质源表；现在键名知识已经全部上移到 `ClassMetaInfo`，这里只剩
        /// 「这张表的类型与键集合是否自洽」这一条场景策略，因此只吃值树。
        bool ValidateMaterialTables(const hgl::serialize::Value &root,
                                    const SceneSpec &spec)
        {
            const hgl::serialize::Value *materials = root.FindFieldExact("materials");

            if (!materials)
                return spec.materials.GetCount() == 0;

            if (!materials->IsArray()
                || materials->GetElementCount() != spec.materials.GetCount())
                return false;

            for (int i = 0; i < spec.materials.GetCount(); ++i)
            {
                const hgl::serialize::Value *source = materials->GetElement(i);

                // `type` 有类内缺省值，缺键会静默落到 PBRSurface；作者侧要求显式写出。
                if (!source || !source->IsTable() || !source->FindFieldExact("type"))
                    return false;

                if (!ValidateMaterialKeys(*source, spec.materials[i]->type))
                    return false;
            }

            return true;
        }

        /// 纯 `SceneSpec` 语义校验：相机参数、清屏色、材质的引用与取值、节点图。
        /// 不碰任何后端类型，TOML 与 FlatBuffers 两条路径共用同一份规则。
        bool ValidateRelationships(const SceneSpec &spec)
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

                if (!IsValidMaterialType(material.type)
                    || !IsFinite(material.base_color)
                    || !IsFinite(material.color)
                    || !IsFinite(material.trans_color))
                    return false;

                for (int j = 0; j < material.textures.GetCount(); ++j)
                {
                    const SceneTextureSpec &texture = *material.textures[j];

                    if (texture.name.IsEmpty() || texture.uri.IsEmpty())
                        return false;

                    for (int k = 0; k < j; ++k)
                        if (texture.name == material.textures[k]->name)
                            return false;
                }
            }

            const auto &nodes = spec.nodes;
            for (int i = 0; i < nodes.GetCount(); ++i)
            {
                const SceneNodeSpec &node = *nodes[i];
                if (node.name.IsEmpty()
                    || !IsValidGeometryReference(node)
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
        out_spec = SceneSpec{};

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

        hgl::serialize::Value          root;
        hgl::serialize::TomlParseError toml_error;

        if (!hgl::serialize::FromTomlText(content, size, root, &toml_error))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: TOML parse error in ")
                      + OSString(debug_name) + OS_TEXT(": ")
                      + ToOSString(toml_error.Format().c_str()));
            return false;
        }

        if (!root.IsTable())
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: root is not a table in ")
                      + OSString(debug_name));
            return false;
        }

        // `schema` 先单独看：它决定这张字段表还能不能按当前契约解释。
        {
            const hgl::serialize::Value *schema_node = root.FindFieldExact("schema");
            int64_t                      schema_value = 0;

            if (!schema_node || !schema_node->GetInt(schema_value) || schema_value != 1)
            {
                MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: unsupported schema in ")
                          + OSString(debug_name));
                return false;
            }
        }

        // `nodes` 没有类内缺省值，必须显式给出且是数组。
        {
            const hgl::serialize::Value *nodes_node = root.FindFieldExact("nodes");

            if (!nodes_node || !nodes_node->IsArray())
            {
                MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: missing 'nodes' array in ")
                          + OSString(debug_name));
                return false;
            }
        }

        // `materials` 可以整体省略，但一旦出现就必须是数组。
        {
            const hgl::serialize::Value *materials_node = root.FindFieldExact("materials");

            if (materials_node && !materials_node->IsArray())
            {
                MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: materials must be an array in ")
                          + OSString(debug_name));
                return false;
            }
        }

        // 必须在键校验之前：`[materials.textures]` 的槽位名在 `SceneTextureSpec` 里
        // 并不存在，展开成数组之后键校验看到的才是 `name` / `uri` / `required`。
        if (!NormalizeTextureTables(root))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: invalid material textures table in ")
                      + OSString(debug_name));
            return false;
        }

        // 严格键校验的唯一真源是生成的 `ClassMetaInfo`。一层表、表数组、子表全靠
        // `toml_key` + `struct_meta` 递归判定，本文件不再持有任何键名白名单。
        {
            AnsiString bad_path;
            AnsiString bad_key;

            if (!hgl::serialize::ValidateKeysByMeta(
                    root, hgl::asset::generated::Class_SceneSpec,
                    AnsiString(), bad_path, bad_key))
            {
                if (bad_path.IsEmpty())
                {
                    MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: unknown key '")
                              + ToOSString(bad_key.c_str()) + OS_TEXT("' in ")
                              + OSString(debug_name));
                }
                else
                {
                    MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: unknown key '")
                              + ToOSString(bad_key.c_str()) + OS_TEXT("' at '")
                              + ToOSString(bad_path.c_str()) + OS_TEXT("' in ")
                              + OSString(debug_name));
                }
                return false;
            }
        }

        // 逐字段解析全部来自 `SceneSerialize.gen.h`：本文件不认识任何键名、
        // 也不认识任何枚举拼写。
        {
            hgl::serialize::Archive archive;

            if (!hgl::serialize::ReadFromValue(root, out_spec, &archive))
            {
                MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: field '")
                          + ToOSString(archive.FailedPath()) + OS_TEXT("' is invalid (")
                          + ToOSString(archive.ErrorMessage()) + OS_TEXT(") in ")
                          + OSString(debug_name));
                return false;
            }
        }

        if (out_spec.name.IsEmpty())
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: missing 'name' in ")
                      + OSString(debug_name));
            return false;
        }

        // `enabled` 的含义是「作者写了这张表」，与表里的 `enabled` 键无关。
        if (root.FindFieldExact("camera"))
            out_spec.camera.enabled = true;
        if (root.FindFieldExact("clear_color"))
            out_spec.clear_color.enabled = true;

        // 以下全是**跨字段策略**，不属于「字段规约」，因此留在解析层。
        if (!ValidateMaterialTables(root, out_spec))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: invalid material table in ")
                      + OSString(debug_name));
            return false;
        }

        if (!ValidateRelationships(out_spec))
        {
            MLogError(SceneAsset, OS_TEXT("ParseSceneSpec: invalid material/node reference, type, or transform in ")
                      + OSString(debug_name));
            return false;
        }

        return true;
    }
}
