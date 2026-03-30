#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/mtl/ShaderVariableType.h>
#include <hgl/mtl/SurfaceType.h>

#include <string>
#include <vector>

namespace hgl::graph::mtl
{

enum class MaterialParamValueType : uint8
{
    Integer = 0,
    Unsigned,
    Float,
    Boolean,
    String,

    ENUM_CLASS_RANGE(Integer, String)
};

struct MaterialSlotPolicy
{
    enum class IndexSource : uint8
    {
        None = 0,
        Constant,
        MaterialInstanceField,
        MaterialInstanceTextureID,

        ENUM_CLASS_RANGE(None, MaterialInstanceTextureID)
    };

    SamplerSlot slot = SamplerSlot::BaseColor;
    bool required = false;

    std::vector<TextureSourceMode> allowed_modes;
    TextureSourceMode default_mode = TextureSourceMode::Simple;
    std::vector<TextureSourceMode> fallback_chain;

    IndexSource index_source = IndexSource::None;
    std::string index_field_name;
    int32 index_constant = 0;
};

struct MaterialParamSpec
{
    std::string name;
    ShaderVariableType type;

    MaterialParamValueType default_value_type = MaterialParamValueType::String;
    std::string default_value;

    bool has_min = false;
    bool has_max = false;
    double min_value = 0.0;
    double max_value = 0.0;
};

struct MaterialRuntimeHints
{
    std::vector<uint8> lod_chain;
};

struct MaterialImplementationCandidate
{
    struct SlotModeRequirement
    {
        SamplerSlot slot = SamplerSlot::BaseColor;
        TextureSourceMode mode = TextureSourceMode::Simple;
    };

    std::string impl_id;
    hgl::graph::SurfaceType surface = hgl::graph::SurfaceType::Standard;

    std::vector<SlotModeRequirement> slot_mode_requirements;
};

struct MaterialProfileAsset
{
    std::string profile_id;
    MaterialPreset semantic_preset = MaterialPreset::Standard;
    uint32 schema_version = 1;

    std::vector<MaterialSlotPolicy> slots;
    std::vector<MaterialParamSpec> instance_params;
    MaterialRuntimeHints runtime_hints;
    std::vector<MaterialImplementationCandidate> implementations;
};

bool ValidateMaterialProfileAsset(const MaterialProfileAsset &asset,
                                  std::vector<std::string> &diagnostics);

} // namespace hgl::graph::mtl
