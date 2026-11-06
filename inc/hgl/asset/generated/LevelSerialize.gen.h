// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，
//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/graph/asset/LevelSpec.h>
#include <hgl/asset/generated/LevelMeta.gen.h>

namespace hgl::serialize
{
    template<> struct EnumTraits<hgl::graph::LevelMaterialType>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::LevelMaterialType v)
        { return hgl::asset::generated::AssetEnumName_LevelMaterialType(v); }
        static bool FromName(const char *s, hgl::graph::LevelMaterialType &out)
        { return hgl::asset::generated::AssetEnumParse_LevelMaterialType(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::LevelCameraMode>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::LevelCameraMode v)
        { return hgl::asset::generated::AssetEnumName_LevelCameraMode(v); }
        static bool FromName(const char *s, hgl::graph::LevelCameraMode &out)
        { return hgl::asset::generated::AssetEnumParse_LevelCameraMode(s, out); }
    };

    template<> struct StructSerializer<hgl::graph::LevelTransformSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "position", v.position);
            ar.Field(2, "rotation_axis", v.rotation_axis);
            ar.Field(3, "rotation_angle_deg", v.rotation_angle_deg);
            ar.Field(4, "scale", v.scale);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_LevelTransformSpec; }
    };

    template<> struct StructSerializer<hgl::graph::LevelTextureSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "name", v.name);
            ar.Field(2, "uri", v.uri);
            ar.Field(3, "required", v.required);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_LevelTextureSpec; }
    };

    template<> struct StructSerializer<hgl::graph::LevelMaterialSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "name", v.name);
            ar.Field(2, "type", v.type);
            ar.Field(3, "base_color", v.base_color);
            ar.Field(4, "metallic", v.metallic);
            ar.Field(5, "roughness", v.roughness);
            ar.Field(6, "normal_scale", v.normal_scale);
            ar.Field(7, "fresnel", v.fresnel);
            ar.Field(8, "color", v.color);
            ar.Field(9, "trans_color", v.trans_color);
            ar.Field(10, "clearcoat", v.clearcoat);
            ar.Field(11, "clearcoat_roughness", v.clearcoat_roughness);
            ar.Field(12, "sheen_color", v.sheen_color);
            ar.Field(13, "sheen_roughness", v.sheen_roughness);
            ar.Field(14, "anisotropy", v.anisotropy);
            ar.Field(15, "subsurface_power", v.subsurface_power);
            ar.Field(16, "thickness", v.thickness);
            ar.Field(17, "textures", v.textures);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_LevelMaterialSpec; }
    };

    template<> struct StructSerializer<hgl::graph::LevelNodeSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "name", v.name);
            ar.Field(2, "parent", v.parent);
            ar.Field(3, "geometry", v.geometry);
            ar.Field(4, "model", v.model);
            ar.Field(5, "recipe", v.recipe);
            ar.Field(6, "material", v.material);
            ar.Field(7, "mobility", v.mobility);
            ar.Field(8, "visible", v.visible);
            ar.Field(9, "transform", v.transform);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_LevelNodeSpec; }
    };

    template<> struct StructSerializer<hgl::graph::LevelCameraSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "enabled", v.enabled);
            ar.Field(2, "mode", v.mode);
            ar.Field(3, "position", v.position);
            ar.Field(4, "target", v.target);
            ar.Field(5, "distance", v.distance);
            ar.Field(6, "yaw", v.yaw);
            ar.Field(7, "pitch", v.pitch);
            ar.Field(8, "fov_deg", v.fov_deg);
            ar.Field(9, "near_plane", v.near_plane);
            ar.Field(10, "far_plane", v.far_plane);
            ar.Field(11, "main", v.main);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_LevelCameraSpec; }
    };

    template<> struct StructSerializer<hgl::graph::LevelClearColorSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "enabled", v.enabled);
            ar.Field(2, "value", v.value);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_LevelClearColorSpec; }
    };

    template<> struct StructSerializer<hgl::graph::LevelSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "schema", v.schema);
            ar.Field(2, "name", v.name);
            ar.Field(3, "description", v.description);
            ar.Field(4, "camera", v.camera);
            ar.Field(5, "clear_color", v.clear_color);
            ar.Field(6, "materials", v.materials);
            ar.Field(7, "nodes", v.nodes);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_LevelSpec; }
    };

}//namespace hgl::serialize
