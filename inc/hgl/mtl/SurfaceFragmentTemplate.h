#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/MaterialVariantRow.h>
#include <cstddef>

namespace hgl::graph::mtl
{
    struct SurfaceFragmentTemplate
    {
        const char *name = "";
        MaterialPreset preset = MaterialPreset::PureColor3D;
        SurfaceType surface_type = SurfaceType::Unlit;
        SurfaceShadingModel surface_model = SurfaceShadingModel::Unknown;
        LightingModel lighting_model = LightingModel::Lambert;
        uint32 required_tex_slots_mask = 0;
        uint32 optional_tex_slots_mask = 0;
        uint32 required_sampler_feature_bits = 0;
        const char *surface_path = "";
        const char *fs_template_path = "";
        ShaderStageFeatureDesc fs_features{};
        MaterialResourceRequirements resource_contract{};
        StaticMaterialDefIdHint def_hint = StaticMaterialDefIdHint::None;
    };

    extern const SurfaceFragmentTemplate kSurfaceFragmentTemplates[];
    extern const size_t                  kSurfaceFragmentTemplatesCount;
}
