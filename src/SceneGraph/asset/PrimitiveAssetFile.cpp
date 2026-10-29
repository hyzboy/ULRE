#include <hgl/graph/asset/PrimitiveAssetFile.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/type/Smart.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>

#include <toml/toml.hpp>
#include <string>
#include <cstring>

DEFINE_LOGGER_MODULE(PrimitiveAsset)

namespace hgl::graph
{
    namespace
    {
        bool ReadBool(const toml::value &table, const char *key, bool def_val)
        {
            if (!table.contains(key))
                return def_val;
            const auto &v = table.at(key);
            return v.is_boolean() ? v.as_boolean() : def_val;
        }

        uint32_t ReadUInt(const toml::value &table, const char *key, uint32_t def_val)
        {
            if (!table.contains(key))
                return def_val;
            const auto &v = table.at(key);
            if (!v.is_integer())
                return def_val;
            const int64_t val = v.as_integer();
            return (val >= 0) ? static_cast<uint32_t>(val) : def_val;
        }

        bool ReadString(const toml::value &table, const char *key, std::string &out)
        {
            if (!table.contains(key))
                return false;
            const auto &v = table.at(key);
            if (!v.is_string())
                return false;
            out = v.as_string();
            return true;
        }
    }//namespace

    const char *GetPrimitiveVariantPurposeName(const PrimitiveVariantPurpose purpose)
    {
        switch (purpose)
        {
        case PrimitiveVariantPurpose::Surface:      return "surface";
        case PrimitiveVariantPurpose::DepthOnly:    return "depth_only";
        case PrimitiveVariantPurpose::ShadowCaster: return "shadow_caster";
        case PrimitiveVariantPurpose::Picking:      return "picking";
        case PrimitiveVariantPurpose::Debug:        return "debug";
        default:                                    return "surface";
        }
    }

    bool ParsePrimitiveVariantPurpose(const char *name, PrimitiveVariantPurpose &out_purpose)
    {
        if (!name || !*name)
            return false;

        if (hgl::stricmp(name, "surface") == 0)        { out_purpose = PrimitiveVariantPurpose::Surface;      return true; }
        if (hgl::stricmp(name, "depth_only") == 0)     { out_purpose = PrimitiveVariantPurpose::DepthOnly;    return true; }
        if (hgl::stricmp(name, "shadow_caster") == 0)  { out_purpose = PrimitiveVariantPurpose::ShadowCaster; return true; }
        if (hgl::stricmp(name, "picking") == 0)        { out_purpose = PrimitiveVariantPurpose::Picking;      return true; }
        if (hgl::stricmp(name, "debug") == 0)          { out_purpose = PrimitiveVariantPurpose::Debug;        return true; }

        return false;
    }

    bool ParsePrimitiveAssetSpec(const char *content, size_t size,
                                 const os_char *debug_name,
                                 PrimitiveAssetSpec &out_spec)
    {
        if (!content || size == 0)
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: empty content for ") + OSString(debug_name));
            return false;
        }

        toml::value root;
        try
        {
            root = toml::parse_str(std::string(content, size));
        }
        catch (const toml::exception &e)
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: TOML parse error in ")
                     + OSString(debug_name) + OS_TEXT(": ") + ToOSString(e.what()));
            return false;
        }

        if (!root.is_table())
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: root is not a table in ") + OSString(debug_name));
            return false;
        }

        const uint32_t schema = ReadUInt(root, "schema", 0);
        if (schema != 1)
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: unsupported schema in ") + OSString(debug_name));
            return false;
        }

        std::string name_str;
        if (ReadString(root, "name", name_str))
            out_spec.name = AnsiString(name_str.c_str(), static_cast<int>(name_str.size()));

        std::string geometry_str;
        if (!ReadString(root, "geometry", geometry_str) || geometry_str.empty())
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: missing 'geometry' in ") + OSString(debug_name));
            return false;
        }
        out_spec.geometry_path = ToOSString(geometry_str);

        std::string recipe_str;
        if (!ReadString(root, "recipe", recipe_str) || recipe_str.empty())
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: missing 'recipe' in ") + OSString(debug_name));
            return false;
        }
        out_spec.recipe_name = AnsiString(recipe_str.c_str(), static_cast<int>(recipe_str.size()));

        std::string primitive_str;
        if (ReadString(root, "primitive_type", primitive_str))
        {
            out_spec.primitive_type = ParsePrimitiveType(primitive_str.c_str());

            if (out_spec.primitive_type == PrimitiveType::Error)
            {
                MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: unknown primitive_type '")
                         + ToOSString(primitive_str) + OS_TEXT("' in ") + OSString(debug_name));
                return false;
            }
        }

        if (root.contains("variants"))
        {
            const toml::value &variants = root.at("variants");

            if (!variants.is_array())
            {
                MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: 'variants' must be an array in ") + OSString(debug_name));
                return false;
            }

            for (const toml::value &item : variants.as_array())
            {
                if (!item.is_table())
                {
                    MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: variant entry must be a table in ") + OSString(debug_name));
                    return false;
                }

                PrimitiveVariantSpec *variant = out_spec.extra_variants.Create();

                if (!variant)
                {
                    MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: out of memory for variants in ") + OSString(debug_name));
                    return false;
                }

                std::string variant_recipe;
                if (!ReadString(item, "recipe", variant_recipe) || variant_recipe.empty())
                {
                    MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: variant missing 'recipe' in ") + OSString(debug_name));
                    return false;
                }

                variant->recipe_name = AnsiString(variant_recipe.c_str(), static_cast<int>(variant_recipe.size()));

                std::string purpose_str;
                if (ReadString(item, "purpose", purpose_str))
                {
                    if (!ParsePrimitiveVariantPurpose(purpose_str.c_str(), variant->purpose))
                    {
                        MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: unknown variant purpose '")
                                 + ToOSString(purpose_str) + OS_TEXT("' in ") + OSString(debug_name));
                        return false;
                    }
                }

                if (variant->purpose == PrimitiveVariantPurpose::Surface)
                {
                    // Surface 变体就是主配方，重复声明会与 recipe 打架
                    MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: extra variant must not use surface purpose in ")
                             + OSString(debug_name));
                    return false;
                }

                variant->lod_index = static_cast<uint16_t>(ReadUInt(item, "lod", 0));
            }
        }

        if (!out_spec.IsValid())
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: incomplete spec in ") + OSString(debug_name));
            return false;
        }

        return true;
    }

    bool LoadPrimitiveAssetSpecFromTOML(const OSString &filename, PrimitiveAssetSpec &out_spec)
    {
        hgl::io::OpenFileInputStream opener(filename);

        if (!opener)
        {
            MLogError(PrimitiveAsset, OS_TEXT("LoadPrimitiveAssetSpecFromTOML: cannot open file ") + filename);
            return false;
        }

        const int64 size = opener->GetSize();

        if (size <= 0 || size > 16 * 1024 * 1024)
        {
            MLogError(PrimitiveAsset, OS_TEXT("LoadPrimitiveAssetSpecFromTOML: invalid file size in ") + filename);
            return false;
        }

        hgl::AutoDeleteArray<char> buffer(static_cast<size_t>(size) + 1);

        if (!buffer || opener->Read(buffer.data(), size) != size)
        {
            MLogError(PrimitiveAsset, OS_TEXT("LoadPrimitiveAssetSpecFromTOML: read failed in ") + filename);
            return false;
        }

        buffer[static_cast<size_t>(size)] = '\0';

        return ParsePrimitiveAssetSpec(buffer.data(), static_cast<size_t>(size), filename.c_str(), out_spec);
    }
}//namespace hgl::graph
