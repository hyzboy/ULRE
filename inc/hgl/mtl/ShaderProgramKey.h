#pragma once

#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/common/PrimitiveTypeDef.h>
#include <hgl/type/FNV1a.h>

namespace hgl::graph::mtl
{
    struct VertexProgramKey
    {
        GeometryMode       geometry_mode = GeometryMode::Mesh3D;
        PositionProviderId position_provider = PositionProviderId::DirectVec3;
        uint32             vertex_attribute_feature_bits = 0;
        PrimitiveType      primitive_type = PrimitiveType::Triangles;
        bool               has_local_to_world = false;

        uint64 Hash() const noexcept;
        bool operator==(const VertexProgramKey &) const noexcept = default;
    };

    struct FragmentProgramKey
    {
        SurfaceType          surface_type = SurfaceType::Unlit;
        RenderAlphaMode      blend_mode = RenderAlphaMode::Opaque;
        LightingModel        lighting_model = LightingModel::Lambert;
        SkyLightAmbientModel sky_ambient_model = SkyLightAmbientModel::Simple;
        uint32               texture_source_bits = 0;
        uint32               sampler_feature_bits = 0;
        uint32               extra_feature_bits = 0;
        uint64               effective_feature_mask = 0;

        uint64 Hash() const noexcept;
        bool operator==(const FragmentProgramKey &) const noexcept = default;
    };

    VertexProgramKey BuildVertexProgramKey(const MaterialVariantKey &key,
                                           const PrimitiveType primitive_type,
                                           const bool has_local_to_world) noexcept;

    FragmentProgramKey BuildFragmentProgramKey(const MaterialVariantKey &key) noexcept;
}

namespace std
{
    template<>
    struct hash<hgl::graph::mtl::VertexProgramKey>
    {
        size_t operator()(const hgl::graph::mtl::VertexProgramKey &k) const noexcept
        {
            return static_cast<size_t>(k.Hash());
        }
    };

    template<>
    struct hash<hgl::graph::mtl::FragmentProgramKey>
    {
        size_t operator()(const hgl::graph::mtl::FragmentProgramKey &k) const noexcept
        {
            return static_cast<size_t>(k.Hash());
        }
    };
}
