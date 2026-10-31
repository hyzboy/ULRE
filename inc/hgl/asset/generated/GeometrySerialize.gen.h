// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，
//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/graph/asset/GeometryAssetSpec.h>
#include <hgl/asset/generated/GeometryMeta.gen.h>

namespace hgl::serialize
{
    template<> struct EnumTraits<hgl::graph::NTBType>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::NTBType v)
        { return hgl::asset::generated::AssetEnumName_NTBType(v); }
        static bool FromName(const char *s, hgl::graph::NTBType &out)
        { return hgl::asset::generated::AssetEnumParse_NTBType(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::VertexNormalFormat>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::VertexNormalFormat v)
        { return hgl::asset::generated::AssetEnumName_VertexNormalFormat(v); }
        static bool FromName(const char *s, hgl::graph::VertexNormalFormat &out)
        { return hgl::asset::generated::AssetEnumParse_VertexNormalFormat(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::GeometryAssetKind>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::GeometryAssetKind v)
        { return hgl::asset::generated::AssetEnumName_GeometryAssetKind(v); }
        static bool FromName(const char *s, hgl::graph::GeometryAssetKind &out)
        { return hgl::asset::generated::AssetEnumParse_GeometryAssetKind(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::BuiltinShapeKind>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::BuiltinShapeKind v)
        { return hgl::asset::generated::AssetEnumName_BuiltinShapeKind(v); }
        static bool FromName(const char *s, hgl::graph::BuiltinShapeKind &out)
        { return hgl::asset::generated::AssetEnumParse_BuiltinShapeKind(s, out); }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinSphereParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "slices", v.slices);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinSphereParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinDomeParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "slices", v.slices);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinDomeParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinCubeParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "segments_x", v.segments_x);
            ar.Field(2, "segments_y", v.segments_y);
            ar.Field(3, "segments_z", v.segments_z);
            ar.Field(4, "ntb", v.ntb);
            ar.Field(5, "tex_coord", v.tex_coord);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinCubeParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinPlaneGridParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "grid_x", v.grid_x);
            ar.Field(2, "grid_y", v.grid_y);
            ar.Field(3, "sub_x", v.sub_x);
            ar.Field(4, "sub_y", v.sub_y);
            ar.Field(5, "lum", v.lum);
            ar.Field(6, "sub_lum", v.sub_lum);
            ar.Field(7, "is_3d", v.is_3d);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinPlaneGridParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinCylinderParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "half_extend", v.half_extend);
            ar.Field(2, "radius", v.radius);
            ar.Field(3, "slices", v.slices);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinCylinderParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinHollowCylinderParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "half_extend", v.half_extend);
            ar.Field(2, "inner_radius", v.inner_radius);
            ar.Field(3, "outer_radius", v.outer_radius);
            ar.Field(4, "slices", v.slices);
            ar.Field(5, "cap_radial_tiles", v.cap_radial_tiles);
            ar.Field(6, "cap_angular_tiles", v.cap_angular_tiles);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinHollowCylinderParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinTorusParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "inner_radius", v.inner_radius);
            ar.Field(2, "outer_radius", v.outer_radius);
            ar.Field(3, "slices", v.slices);
            ar.Field(4, "stacks", v.stacks);
            ar.Field(5, "uv_scale_x", v.uv_scale_x);
            ar.Field(6, "uv_scale_y", v.uv_scale_y);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinTorusParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinConeParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "half_extend", v.half_extend);
            ar.Field(2, "radius", v.radius);
            ar.Field(3, "slices", v.slices);
            ar.Field(4, "stacks", v.stacks);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinConeParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinCapsuleParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "half_height", v.half_height);
            ar.Field(2, "radius", v.radius);
            ar.Field(3, "slices", v.slices);
            ar.Field(4, "stacks", v.stacks);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinCapsuleParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinRectangleParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "left", v.left);
            ar.Field(2, "top", v.top);
            ar.Field(3, "width", v.width);
            ar.Field(4, "height", v.height);
            ar.Field(5, "segments_x", v.segments_x);
            ar.Field(6, "segments_y", v.segments_y);
            ar.Field(7, "ntb", v.ntb);
            ar.Field(8, "tex_coord", v.tex_coord);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinRectangleParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinAxisParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "size", v.size);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinAxisParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinBoundingBoxParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "ntb", v.ntb);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinBoundingBoxParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinHexSphereParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "subdivisions", v.subdivisions);
            ar.Field(2, "radius", v.radius);
            ar.Field(3, "uv_scale_x", v.uv_scale_x);
            ar.Field(4, "uv_scale_y", v.uv_scale_y);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinHexSphereParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinTaperedCapsuleParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "half_height", v.half_height);
            ar.Field(2, "bottom_radius", v.bottom_radius);
            ar.Field(3, "top_radius", v.top_radius);
            ar.Field(4, "slices", v.slices);
            ar.Field(5, "stacks", v.stacks);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinTaperedCapsuleParams; }
    };

    template<> struct StructSerializer<hgl::graph::BuiltinFrustumParams>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "bottom_radius", v.bottom_radius);
            ar.Field(2, "top_radius", v.top_radius);
            ar.Field(3, "height", v.height);
            ar.Field(4, "slices", v.slices);
            ar.Field(5, "generate_caps", v.generate_caps);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_BuiltinFrustumParams; }
    };

    template<> struct StructSerializer<hgl::graph::GeometryVertexFormatSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "normal", v.normal);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_GeometryVertexFormatSpec; }
    };

    template<> struct StructSerializer<hgl::graph::GeometryBuiltinParamsSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "sphere", v.sphere);
            ar.Field(2, "dome", v.dome);
            ar.Field(3, "cube", v.cube);
            ar.Field(4, "plane_grid", v.plane_grid);
            ar.Field(5, "cylinder", v.cylinder);
            ar.Field(6, "hollow_cylinder", v.hollow_cylinder);
            ar.Field(7, "torus", v.torus);
            ar.Field(8, "cone", v.cone);
            ar.Field(9, "capsule", v.capsule);
            ar.Field(10, "rectangle", v.rectangle);
            ar.Field(11, "axis", v.axis);
            ar.Field(12, "bounding_box", v.bounding_box);
            ar.Field(13, "hex_sphere", v.hex_sphere);
            ar.Field(14, "tapered_capsule", v.tapered_capsule);
            ar.Field(15, "frustum", v.frustum);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_GeometryBuiltinParamsSpec; }
    };

    template<> struct StructSerializer<hgl::graph::GeometryAssetSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "schema", v.schema);
            ar.Field(2, "name", v.name);
            ar.Field(3, "type", v.kind);
            ar.Field(4, "shape", v.shape);
            ar.Field(5, "builtin", v.params);
            ar.Field(6, "file", v.file_path);
            ar.Field(7, "vertex_format", v.vertex_format);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_GeometryAssetSpec; }
    };

}//namespace hgl::serialize
