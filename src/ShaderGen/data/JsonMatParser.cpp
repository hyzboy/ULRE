#include "JsonMatParser.h"

#include <hgl/mtl/new/SurfaceType.h>
#include <hgl/mtl/new/MaterialVariantKey.h>
#include <hgl/mtl/new/BlendMode.h>
#include <hgl/mtl/new/PassType.h>
#include <hgl/mtl/DescriptorBindingContract.h>
#include <hgl/mtl/SamplerName.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/PrimitiveTypeDef.h>
#include <hgl/common/TextureSamplerTypeDef.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <cstring>
#include <climits>
#include <iostream>

namespace hgl::graph::mtl
{
namespace
{
    // -----------------------------------------------------------------------
    //  Enum parsers
    // -----------------------------------------------------------------------

    SurfaceType ParseSurfaceType(const std::string& s)
    {
        // 3D surface types
        for (size_t i = 0; i < std::size(SurfaceTypeNames); ++i)
            if (s == SurfaceTypeNames[i])
                return static_cast<SurfaceType>(i);

        // 2D surface types (offset = 20)
        for (size_t i = 0; i < std::size(SurfaceType2DNames); ++i)
            if (s == SurfaceType2DNames[i])
                return static_cast<SurfaceType>(static_cast<uint8_t>(SurfaceType::PureColor2D) + i);

        std::cerr << "[JsonMatParser] Unknown surface_type: " << s << "\n";
        return SurfaceType::Unlit;
    }

    GeometryMode ParseGeometryMode(const std::string& s)
    {
        constexpr const char* names[] = {
            "Mesh3D", "Quad2D", "ScreenRect", "BillboardCameraFacing", "BillboardAxisLocked"
        };
        for (size_t i = 0; i < std::size(names); ++i)
            if (s == names[i])
                return static_cast<GeometryMode>(i);

        std::cerr << "[JsonMatParser] Unknown geometry_mode: " << s << "\n";
        return GeometryMode::Mesh3D;
    }

    BlendMode ParseBlendMode(const std::string& s)
    {
        constexpr const char* names[] = {
            "Opaque", "Masked", "Transparent", "Dither", "AlphaToCoverage"
        };
        for (size_t i = 0; i < std::size(names); ++i)
            if (s == names[i])
                return static_cast<BlendMode>(i);

        std::cerr << "[JsonMatParser] Unknown blend_mode: " << s << "\n";
        return BlendMode::Opaque;
    }

    PassType ParsePassType(const std::string& s)
    {
        constexpr const char* names[] = {
            "ForwardOpaque", "ForwardMasked", "ForwardTransparent",
            "ForwardDither", "ForwardA2C",
            "ShadowOpaque", "ShadowMasked",
            "EarlyZSolid", "EarlyZMasked"
        };
        for (size_t i = 0; i < std::size(names); ++i)
            if (s == names[i])
                return static_cast<PassType>(i);

        std::cerr << "[JsonMatParser] Unknown pass_type: " << s << "\n";
        return PassType::ForwardOpaque;
    }

    UBODescriptorSemantic ParseUBOSemantic(const std::string& s)
    {
        for (size_t i = 0; i < UBODescriptorSemanticCount; ++i)
            if (s == UBODescriptorSemanticNameList[i])
                return static_cast<UBODescriptorSemantic>(i);

        std::cerr << "[JsonMatParser] Unknown UBO descriptor: " << s << "\n";
        return UBODescriptorSemantic::Unknown;
    }

    SSBODescriptorSemantic ParseSSBOSemantic(const std::string& s)
    {
        for (size_t i = 0; i < SSBODescriptorSemanticCount; ++i)
            if (s == SSBODescriptorSemanticNameList[i])
                return static_cast<SSBODescriptorSemantic>(i);

        std::cerr << "[JsonMatParser] Unknown SSBO descriptor: " << s << "\n";
        return SSBODescriptorSemantic::Unknown;
    }

    SamplerSlot ParseSamplerSlot(const std::string& s)
    {
        for (size_t i = 0; i < SamplerSlotCount; ++i)
            if (s == SamplerSlotNameList[i])
                return static_cast<SamplerSlot>(i);

        std::cerr << "[JsonMatParser] Unknown sampler slot: " << s << "\n";
        return SamplerSlot::BaseColor;
    }

    SamplerType ParseSamplerType(const std::string& s)
    {
        // SamplerTypeName[] is indexed: index 0 = "samplerError", 1 = "sampler1D", 2 = "sampler2D"...
        // but enum value Sampler2D = 2; use the friendly name "Sampler2D" not the GLSL name.
        constexpr const char* friendly_names[] = {
            "Error",
            "Sampler1D", "Sampler2D", "Sampler3D",
            "SamplerCube", "Sampler2DRect",
            "Sampler1DArray", "Sampler2DArray",
            "SamplerCubeArray",
            "SamplerBuffer",
            "Sampler2DMS", "Sampler2DMSArray",
            "Sampler1DShadow", "Sampler2DShadow",
            "SamplerCubeShadow", "Sampler2DRectShadow",
            "Sampler1DArrayShadow", "Sampler2DArrayShadow",
            "SamplerCubeArrayShadow"
        };
        for (size_t i = 0; i < std::size(friendly_names); ++i)
            if (s == friendly_names[i])
                return static_cast<SamplerType>(i);

        std::cerr << "[JsonMatParser] Unknown sampler type: " << s << "\n";
        return SamplerType::Sampler2D;
    }

    // -----------------------------------------------------------------------
    //  Parse vertex_entries array
    // -----------------------------------------------------------------------

    bool ParseVertexEntries(const nlohmann::json& jarr, std::vector<FixedVertexEntry>& out)
    {
        if (!jarr.is_array())
        {
            std::cerr << "[JsonMatParser] vertex_entries must be a JSON array\n";
            return false;
        }
        for (const auto& jent : jarr)
        {
            const std::string attrib_name = jent.value("attrib", "");
            const std::string format_name = jent.value("format", "");

            VertexAttrib attrib = GetVertexAttribByName(attrib_name.c_str());
            if (attrib >= VertexAttrib::RANGE_SIZE)
            {
                std::cerr << "[JsonMatParser] Unknown vertex attrib: " << attrib_name << "\n";
                return false;
            }

            VAType vat{};
            if (!ParseVertexAttribType(&vat, format_name.c_str()))
            {
                std::cerr << "[JsonMatParser] Unknown vertex format: " << format_name << "\n";
                return false;
            }

            out.push_back(FixedVertexEntry{vat, attrib});
        }
        return true;
    }

    // -----------------------------------------------------------------------
    //  Parse texture_samplers object
    // -----------------------------------------------------------------------

    void ParseTextureSamplers(const nlohmann::json& jobj,
                              std::map<SamplerSlot, FixedTextureSamplerDescriptor>& out)
    {
        if (!jobj.is_object()) return;
        for (auto it = jobj.begin(); it != jobj.end(); ++it)
        {
            SamplerSlot slot = ParseSamplerSlot(it.key());
            const auto& jsampler = it.value();

            FixedTextureSamplerDescriptor desc{};
            desc.sampler_type = ParseSamplerType(jsampler.value("type", "Sampler2D"));
            desc.atlas_cols   = jsampler.value("atlas_cols", 0u);
            desc.atlas_rows   = jsampler.value("atlas_rows", 0u);
            out[slot] = desc;
        }
    }

    // -----------------------------------------------------------------------
    //  Parse features object
    // -----------------------------------------------------------------------

    void ParseFeatures(const nlohmann::json& jfeat,
                       std::map<std::string, bool>&        bool_feats,
                       std::map<std::string, int>&         int_feats,
                       std::map<std::string, std::string>& str_feats)
    {
        if (!jfeat.is_object()) return;
        for (auto it = jfeat.begin(); it != jfeat.end(); ++it)
        {
            if (it.value().is_boolean())
                bool_feats[it.key()] = it.value().get<bool>();
            else if (it.value().is_number_integer())
                int_feats[it.key()] = it.value().get<int>();
            else if (it.value().is_string())
                str_feats[it.key()] = it.value().get<std::string>();
        }
    }

    // -----------------------------------------------------------------------
    //  Core parse logic
    // -----------------------------------------------------------------------

    std::optional<MaterialDef> ParseFromJson(const nlohmann::json& j)
    {
        MaterialDef def;

        def.name = j.value("name", "");
        if (def.name.empty())
        {
            std::cerr << "[JsonMatParser] Missing required field: name\n";
            return std::nullopt;
        }

        def.surface_type  = ParseSurfaceType(j.value("surface_type",  "Unlit"));
        def.geometry_mode = ParseGeometryMode(j.value("geometry_mode", "Mesh3D"));
        def.blend_mode    = ParseBlendMode(j.value("blend_mode",    "Opaque"));
        def.pass_type     = ParsePassType(j.value("pass_type",     "ForwardOpaque"));
        def.primitive_type = ParsePrimitiveType(j.value("primitive_type", "Triangles").c_str());

        // vertex_entries
        if (j.contains("vertex_entries"))
        {
            if (!ParseVertexEntries(j["vertex_entries"], def.vertex_entries))
                return std::nullopt;
        }

        // ubo_descriptors
        if (j.contains("ubo_descriptors") && j["ubo_descriptors"].is_array())
        {
            for (const auto& jv : j["ubo_descriptors"])
            {
                auto sem = ParseUBOSemantic(jv.get<std::string>());
                if (sem != UBODescriptorSemantic::Unknown)
                    def.ubo_descriptors.insert(sem);
            }
        }

        // ssbo_descriptors
        if (j.contains("ssbo_descriptors") && j["ssbo_descriptors"].is_array())
        {
            for (const auto& jv : j["ssbo_descriptors"])
            {
                auto sem = ParseSSBOSemantic(jv.get<std::string>());
                if (sem != SSBODescriptorSemantic::Unknown)
                    def.ssbo_descriptors.insert(sem);
            }
        }

        // texture_samplers (optional)
        if (j.contains("texture_samplers"))
            ParseTextureSamplers(j["texture_samplers"], def.texture_samplers);

        // MI
        def.mi_glsl_struct  = j.value("mi_glsl",        "");
        def.mi_struct_bytes = j.value("mi_struct_bytes", 0u);

        // Shader templates
        def.vs_template = j.value("vs_template", "");
        def.fs_template = j.value("fs_template", "");

        // Features
        if (j.contains("features"))
            ParseFeatures(j["features"], def.bool_features, def.int_features, def.string_features);

        if (!def.IsValid())
        {
            std::cerr << "[JsonMatParser] Invalid MaterialDef (name=" << def.name
                      << "): vertex_entries must not be empty\n";
            return std::nullopt;
        }

        return def;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

std::optional<MaterialDef> ParseMaterialDef(const std::string& file_path)
{
    std::ifstream ifs(file_path);
    if (!ifs)
    {
        std::cerr << "[JsonMatParser] Cannot open file: " << file_path << "\n";
        return std::nullopt;
    }

    try
    {
        nlohmann::json j = nlohmann::json::parse(ifs);
        return ParseFromJson(j);
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "[JsonMatParser] JSON parse error in " << file_path
                  << ": " << ex.what() << "\n";
        return std::nullopt;
    }
}

std::optional<MaterialDef> ParseMaterialDefFromString(const std::string& json_text)
{
    try
    {
        nlohmann::json j = nlohmann::json::parse(json_text);
        return ParseFromJson(j);
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "[JsonMatParser] JSON parse error: " << ex.what() << "\n";
        return std::nullopt;
    }
}

} // namespace hgl::graph::mtl
