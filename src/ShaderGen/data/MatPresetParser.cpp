#include "MatPresetParser.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

namespace hgl::graph::mtl
{
namespace
{
    std::optional<MaterialPresetDef> ParseFromJson(const nlohmann::json& j)
    {
        MaterialPresetDef def;

        def.name = j.value("name", "");
        if (def.name.empty())
        {
            std::cerr << "[MatPresetParser] Missing required field: name\n";
            return std::nullopt;
        }

        // lod_chain
        if (j.contains("lod_chain") && j["lod_chain"].is_array())
        {
            for (const auto& jv : j["lod_chain"])
                def.lod_chain.push_back(jv.get<std::string>());
        }
        if (def.lod_chain.empty())
        {
            std::cerr << "[MatPresetParser] lod_chain must have at least one entry\n";
            return std::nullopt;
        }

        // default_params — store as raw JSON string to keep nlohmann_json out of consumer headers
        if (j.contains("default_params") && !j["default_params"].is_null())
            def.default_params_json = j["default_params"].dump();
        else
            def.default_params_json = "{}";

        // default_config (all fields optional, defaults are already set in the struct)
        if (j.contains("default_config") && j["default_config"].is_object())
        {
            const auto& jcfg = j["default_config"];
            def.default_config.with_camera = jcfg.value("with_camera", def.default_config.with_camera);
            def.default_config.with_sky    = jcfg.value("with_sky",    def.default_config.with_sky);
            def.default_config.with_l2w    = jcfg.value("with_l2w",    def.default_config.with_l2w);
        }

        return def;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

std::optional<MaterialPresetDef> ParseMaterialPresetDef(const std::string& file_path)
{
    std::ifstream ifs(file_path);
    if (!ifs)
    {
        std::cerr << "[MatPresetParser] Cannot open file: " << file_path << "\n";
        return std::nullopt;
    }

    try
    {
        nlohmann::json j = nlohmann::json::parse(ifs);
        return ParseFromJson(j);
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "[MatPresetParser] JSON parse error in " << file_path
                  << ": " << ex.what() << "\n";
        return std::nullopt;
    }
}

std::optional<MaterialPresetDef> ParseMaterialPresetDefFromString(const std::string& json_text)
{
    try
    {
        nlohmann::json j = nlohmann::json::parse(json_text);
        return ParseFromJson(j);
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "[MatPresetParser] JSON parse error: " << ex.what() << "\n";
        return std::nullopt;
    }
}

} // namespace hgl::graph::mtl
