#pragma once

// ABI CONTRACT: Field order is frozen. To add a new field: place it before
// _reserved, shrink _reserved by the same size, then update Hash() and the
// sizeof baseline assertion in MaterialKeyTest.
#include <hgl/mtl/MaterialVariantKey.h>   // pulls PassType.h, FNV1a.h, CoreType.h
#include <hgl/mtl/ShaderDataSchema.h>
#include <compare>

namespace hgl::graph::mtl {

    using StaticMaterialDefId = uint16;
    constexpr StaticMaterialDefId kInvalidStaticMaterialDefId = 0;

    // MaterialKey is the stable, serializable identity of a compiled material
    // permutation.  It combines the variant routing key (which shader variant
    // to generate) with the top-level pipeline axes (pass, def, schema,
    // target GLSL/Vulkan/SPIR-V version).
    //
    // Constraints:
    //   - Must remain trivially copyable (used in flat maps and GPU buffers).
    //   - Hash() and operator== define the equality contract.
    //   - operator<=> enables sorted containers (std::map, sorted vectors).
    struct MaterialKey
    {
        MaterialVariantKey      variant{};
        hgl::graph::PassType    pass         = hgl::graph::PassType::ForwardOpaque;
        StaticMaterialDefId     def_id       = kInvalidStaticMaterialDefId;
        ShaderDataSchema        schema       = ShaderDataSchema::None;
        uint16                  glsl_version = 0;
        uint16                  vk_version   = 0;
        uint16                  spv_version  = 0;
        uint64                  _reserved    = 0;   // must stay last; excluded from Hash

        uint64 Hash() const noexcept;

        // operator== is valid here: MaterialVariantKey defines operator==.
        bool operator==(const MaterialKey &) const noexcept = default;

        // operator<=> cannot be defaulted: MaterialVariantKey has no <=>.
        // Implemented manually in MaterialKey.cpp.
        std::strong_ordering operator<=>(const MaterialKey &) const noexcept;
    };

    static_assert(std::is_trivially_copyable_v<MaterialKey>,
                  "MaterialKey must remain trivially copyable for serialization and "
                  "GPU-buffer usage");

} // namespace hgl::graph::mtl

// std::hash specialisation — lets MaterialKey be used directly as an
// unordered_map / unordered_set key.
namespace std {
    template <>
    struct hash<hgl::graph::mtl::MaterialKey>
    {
        size_t operator()(const hgl::graph::mtl::MaterialKey &k) const noexcept
        {
            return static_cast<size_t>(k.Hash());
        }
    };
} // namespace std
