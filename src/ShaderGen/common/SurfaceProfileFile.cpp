#include <hgl/mtl/SurfaceProfileFile.h>

#include <hgl/io/FileInputStream.h>
#include <hgl/type/Smart.h>
#include <toml/toml.hpp>

namespace hgl::graph::mtl
{
    namespace
    {
        bool ReadRequiredString(
            const toml::value &table,
            const char *key,
            AnsiString &out)
        {
            if (!table.is_table()
             || !table.contains(key)
             || !table.at(key).is_string())
                return false;

            const auto &value = table.at(key).as_string();
            if (value.empty())
                return false;

            out = value.c_str();
            return true;
        }

        bool ReadUInt16(
            const toml::value &table,
            const char *key,
            uint16 &out)
        {
            if (!table.is_table()
             || !table.contains(key)
             || !table.at(key).is_integer())
                return false;

            const auto value = table.at(key).as_integer();
            if (value < 0 || value > 0xffff)
                return false;

            out = static_cast<uint16>(value);
            return true;
        }

        bool ParseProfiles(
            const toml::value &root,
            SurfaceProfileFileData &out_data)
        {
            if (!root.contains("profiles")
             || !root.at("profiles").is_array())
                return false;

            const auto &values = root.at("profiles").as_array();
            if (values.empty())
                return false;

            for (const auto &value : values)
            {
                SurfaceImplementationProfile *profile =
                    out_data.profiles.Create();
                if (!profile
                 || !ReadRequiredString(
                        value, "id", profile->profile_name)
                 || !ReadRequiredString(
                        value,
                        "parameter_schema",
                        profile->parameter_schema_name)
                 || !ReadUInt16(
                        value,
                        "schema_version",
                        profile->schema_version)
                 || !ReadUInt16(
                        value, "quality_rank", profile->quality_rank))
                    return false;

                profile->profile_id =
                    GetSurfaceStableID(profile->profile_name);
                profile->parameter_schema_id =
                    GetSurfaceStableID(profile->parameter_schema_name);
            }

            return true;
        }

        bool ParseIntents(
            const toml::value &root,
            SurfaceProfileFileData &out_data)
        {
            if (!root.contains("intents")
             || !root.at("intents").is_array())
                return false;

            const auto &values = root.at("intents").as_array();
            if (values.empty())
                return false;

            for (const auto &value : values)
            {
                SurfaceIntentDefinition *intent =
                    out_data.intents.Create();
                if (!intent
                 || !ReadRequiredString(
                        value, "id", intent->intent_name))
                    return false;

                AnsiString preferred_profile;
                if (!ReadRequiredString(
                        value,
                        "preferred_profile",
                        preferred_profile))
                    return false;
                intent->intent_id = GetSurfaceStableID(intent->intent_name);
                intent->preferred_profile_id =
                    GetSurfaceStableID(preferred_profile);
            }

            return true;
        }

        bool ParseDowngrades(
            const toml::value &root,
            SurfaceProfileFileData &out_data)
        {
            if (!root.contains("downgrades"))
                return true;
            if (!root.at("downgrades").is_array())
                return false;

            for (const auto &value : root.at("downgrades").as_array())
            {
                SurfaceProfileDowngrade downgrade{};
                AnsiString source_profile;
                AnsiString target_profile;
                if (!ReadRequiredString(
                        value,
                        "from",
                        source_profile)
                 || !ReadRequiredString(
                        value,
                        "to",
                        target_profile)
                 || !ReadUInt16(
                        value,
                        "order",
                        downgrade.selection_order))
                    return false;

                downgrade.source_profile_id =
                    GetSurfaceStableID(source_profile);
                downgrade.target_profile_id =
                    GetSurfaceStableID(target_profile);
                out_data.downgrades.Add(downgrade);
            }

            return true;
        }
    }

    const char *GetSurfaceProfileFileParseResultName(
        const SurfaceProfileFileParseResult result) noexcept
    {
        switch (result)
        {
        case SurfaceProfileFileParseResult::Skipped: return "Skipped";
        case SurfaceProfileFileParseResult::OK: return "OK";
        case SurfaceProfileFileParseResult::InvalidValue: return "InvalidValue";
        case SurfaceProfileFileParseResult::UnsupportedSchema: return "UnsupportedSchema";
        }

        return "Unknown";
    }

    SurfaceProfileFileParseResult ParseSurfaceProfileFile(
        const char *content,
        const int content_size,
        SurfaceProfileFileData &out_data) noexcept
    {
        out_data.profiles.Clear();
        out_data.intents.Clear();
        out_data.downgrades.Clear();

        if (!content || content_size <= 0)
            return SurfaceProfileFileParseResult::Skipped;

        try
        {
            const toml::value root = toml::parse_str(
                std::string(content, static_cast<size_t>(content_size)));
            if (!root.is_table()
             || !root.contains("schema")
             || !root.at("schema").is_integer())
                return SurfaceProfileFileParseResult::InvalidValue;

            if (root.at("schema").as_integer()
                != SurfaceProfileSchemaVersion)
                return SurfaceProfileFileParseResult::UnsupportedSchema;

            return ParseProfiles(root, out_data)
                && ParseIntents(root, out_data)
                && ParseDowngrades(root, out_data)
                    ? SurfaceProfileFileParseResult::OK
                    : SurfaceProfileFileParseResult::InvalidValue;
        }
        catch (const toml::exception &)
        {
            return SurfaceProfileFileParseResult::InvalidValue;
        }
    }

    bool LoadSurfaceProfileFile(
        const OSString &path,
        SurfaceProfileFileData &out_data)
    {
        out_data.profiles.Clear();
        out_data.intents.Clear();
        out_data.downgrades.Clear();

        hgl::io::OpenFileInputStream opener(path);
        if (!opener)
            return false;

        const int64 size = opener->GetSize();
        if (size <= 0 || size > 0x7fffffff)
            return false;

        hgl::AutoDeleteArray<char> buffer(static_cast<size_t>(size) + 1);
        if (!buffer || opener->Read(buffer.data(), size) != size)
            return false;
        buffer[static_cast<size_t>(size)] = 0;

        return ParseSurfaceProfileFile(
            buffer.data(),
            static_cast<int>(size),
            out_data) == SurfaceProfileFileParseResult::OK;
    }

    bool RegisterSurfaceProfileFileData(
        const SurfaceProfileFileData &data,
        SurfaceProfileRegistry &registry,
        SurfaceProfileValidationDiagnostic &out_diagnostic)
    {
        out_diagnostic = {};
        if (registry.GetProfileCount() != 0
         || registry.GetIntentCount() != 0
         || registry.GetDowngradeCount() != 0)
        {
            out_diagnostic.error =
                SurfaceProfileValidationError::InvalidProfile;
            return false;
        }

        for (int i = 0; i < data.profiles.GetCount(); ++i)
        {
            if (!data.profiles[i]
             || !registry.RegisterProfile(*data.profiles[i]))
            {
                registry.Clear();
                out_diagnostic.error =
                    SurfaceProfileValidationError::InvalidProfile;
                out_diagnostic.item_index = static_cast<uint32>(i);
                return false;
            }
        }

        for (int i = 0; i < data.intents.GetCount(); ++i)
        {
            if (!data.intents[i]
             || !registry.RegisterIntent(*data.intents[i]))
            {
                registry.Clear();
                out_diagnostic.error =
                    SurfaceProfileValidationError::InvalidIntent;
                out_diagnostic.item_index = static_cast<uint32>(i);
                return false;
            }
        }

        for (int i = 0; i < data.downgrades.GetCount(); ++i)
        {
            if (!registry.RegisterDowngrade(data.downgrades[i]))
            {
                registry.Clear();
                out_diagnostic.error =
                    SurfaceProfileValidationError::InvalidDowngrade;
                out_diagnostic.item_index = static_cast<uint32>(i);
                return false;
            }
        }

        if (!registry.Validate(out_diagnostic))
        {
            registry.Clear();
            return false;
        }

        return true;
    }
}
