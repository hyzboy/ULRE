#pragma once

#include <hgl/asset/AssetAnnotation.h>
#include <hgl/CoreType.h>
#include <hgl/color/Color4f.h>
#include <hgl/math/VectorTypes.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    enum class SceneMaterialType : uint8
    {
        PBRSurface,
        EmissiveSurface,
        TransmissionSurface
    };

    enum class SceneCameraMode : uint8
    {
        ViewModel,
        Free,
        LookAt,
        FirstPerson
    };

    struct HGL_ASSET_STRUCT SceneTransformSpec
    {
        hgl::math::Vector3f position{0.0f};
        hgl::math::Vector3f rotation_axis{0.0f, 0.0f, 1.0f};
        float rotation_angle_deg = 0.0f;
        hgl::math::Vector3f scale{1.0f};
    };

    struct HGL_ASSET_STRUCT SceneTextureSpec
    {
        AnsiString name;
        OSString uri;
        bool required = true;
    };

    struct HGL_ASSET_STRUCT SceneMaterialSpec
    {
        AnsiString name;
        SceneMaterialType type = SceneMaterialType::PBRSurface;
        Color4f base_color = Color4f(1.0f);
        float metallic = 0.0f;
        float roughness = 1.0f;
        float normal_scale = 1.0f;
        float fresnel = 0.04f;
        Color4f color = Color4f(1.0f);
        Color4f trans_color = Color4f(1.0f);
        ManagedArray<SceneTextureSpec> textures;
    };

    struct HGL_ASSET_STRUCT SceneNodeSpec
    {
        AnsiString name;
        AnsiString parent;
        AnsiString geometry;
        AnsiString recipe;
        AnsiString material;
        AnsiString mobility = "movable";
        bool visible = true;
        SceneTransformSpec transform;
    };

    struct HGL_ASSET_STRUCT SceneCameraSpec
    {
        bool enabled = false;
        SceneCameraMode mode = SceneCameraMode::ViewModel;
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

    struct HGL_ASSET_STRUCT SceneClearColorSpec
    {
        bool enabled = false;
        Color4f value{0.0f, 0.0f, 0.0f, 1.0f};
    };

    struct HGL_ASSET_STRUCT SceneSpec
    {
        uint32 schema = 1;
        AnsiString name;
        AnsiString description;
        SceneCameraSpec camera;
        SceneClearColorSpec clear_color;
        ManagedArray<SceneMaterialSpec> materials;
        ManagedArray<SceneNodeSpec> nodes;
    };
}
