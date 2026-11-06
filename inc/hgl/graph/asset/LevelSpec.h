#pragma once

#include <hgl/asset/AssetAnnotation.h>
#include <hgl/CoreType.h>
#include <hgl/color/Color4f.h>
#include <hgl/math/VectorTypes.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    enum class LevelMaterialType : uint8
    {
        PBRSurface,
        EmissiveSurface,
        TransmissionSurface,
        PBRAdvancedSurface
    };

    enum class LevelCameraMode : uint8
    {
        ViewModel,
        Free,
        LookAt,
        FirstPerson
    };

    struct HGL_ASSET_STRUCT LevelTransformSpec
    {
        hgl::math::Vector3f position{0.0f};
        hgl::math::Vector3f rotation_axis{0.0f, 0.0f, 1.0f};
        float rotation_angle_deg = 0.0f;
        hgl::math::Vector3f scale{1.0f};
    };

    struct HGL_ASSET_STRUCT LevelTextureSpec
    {
        AnsiString name;
        OSString uri;
        bool required = true;
    };

    struct HGL_ASSET_STRUCT LevelMaterialSpec
    {
        AnsiString name;
        LevelMaterialType type = LevelMaterialType::PBRSurface;
        Color4f base_color = Color4f(1.0f);
        float metallic = 0.0f;
        float roughness = 1.0f;
        float normal_scale = 1.0f;
        float fresnel = 0.04f;
        Color4f color = Color4f(1.0f);
        Color4f trans_color = Color4f(1.0f);
        // PBRAdvancedSurface 专用（Filament 高级材质扩展；授权侧超集字段，
        // 与 MaterialDataRowSpec 同款：sheen_color 落 GPU 行时打包成 unorm4x8）
        float clearcoat = 0.0f;
        float clearcoat_roughness = 0.0f;
        Color4f sheen_color = Color4f(1.0f);
        float sheen_roughness = 0.0f;
        float anisotropy = 0.0f;
        float subsurface_power = 0.0f;
        float thickness = 0.0f;
        ManagedArray<LevelTextureSpec> textures;
    };

    struct HGL_ASSET_STRUCT LevelNodeSpec
    {
        AnsiString name;
        AnsiString parent;
        AnsiString geometry;
        AnsiString model;   // 复合模型资产引用（`.model` MDL2 包）；与 geometry 二选一
        AnsiString recipe;
        AnsiString material;
        AnsiString mobility = "movable";
        bool visible = true;
        LevelTransformSpec transform;
    };

    struct HGL_ASSET_STRUCT LevelCameraSpec
    {
        bool enabled = false;
        LevelCameraMode mode = LevelCameraMode::ViewModel;
        hgl::math::Vector3f position{0.0f, 0.0f, 5.0f};
        hgl::math::Vector3f target{0.0f};
        float distance = 10.0f;
        float yaw = 0.0f;
        float pitch = 0.0f;
        float fov_deg = 45.0f;
        float near_plane = 0.1f;
        float far_plane = 1000.0f;
        bool main = true;
    };

    struct HGL_ASSET_STRUCT LevelClearColorSpec
    {
        bool enabled = false;
        Color4f value{0.0f, 0.0f, 0.0f, 1.0f};
    };

    struct HGL_ASSET_STRUCT LevelSpec
    {
        uint32 schema = 1;
        AnsiString name;
        AnsiString description;
        LevelCameraSpec camera;
        LevelClearColorSpec clear_color;
        ManagedArray<LevelMaterialSpec> materials;
        ManagedArray<LevelNodeSpec> nodes;
    };
}
