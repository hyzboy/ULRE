// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   语义：键缺失 = 保留 C++ 类内初始值；键存在但类型/长度不符 = 解析失败。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <toml/toml.hpp>
#include <hgl/asset/AssetSerialize.h>
#include <hgl/graph/asset/GeometryAssetSpec.h>

namespace hgl::asset::generated
{
    inline const char *AssetEnumName_NTBType(const hgl::graph::NTBType v)
    {
        switch (v)
        {
        case hgl::graph::NTBType::None: return "None";
        case hgl::graph::NTBType::Normal: return "Normal";
        case hgl::graph::NTBType::NormalTangent: return "NormalTangent";
        case hgl::graph::NTBType::NormalTangentBitangent: return "NormalTangentBitangent";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_NTBType(const char *s, hgl::graph::NTBType &out)
    {
        if (!s) return false;
        if (hgl::strcmp(s, "None") == 0) { out = hgl::graph::NTBType::None; return true; }
        if (hgl::strcmp(s, "Normal") == 0) { out = hgl::graph::NTBType::Normal; return true; }
        if (hgl::strcmp(s, "NormalTangent") == 0) { out = hgl::graph::NTBType::NormalTangent; return true; }
        if (hgl::strcmp(s, "NormalTangentBitangent") == 0) { out = hgl::graph::NTBType::NormalTangentBitangent; return true; }
        return false;
    }

    inline bool ReadBuiltinSphereParams(const toml::value &table, hgl::graph::BuiltinSphereParams &out);
    inline bool ReadBuiltinDomeParams(const toml::value &table, hgl::graph::BuiltinDomeParams &out);
    inline bool ReadBuiltinCubeParams(const toml::value &table, hgl::graph::BuiltinCubeParams &out);
    inline bool ReadBuiltinPlaneGridParams(const toml::value &table, hgl::graph::BuiltinPlaneGridParams &out);
    inline bool ReadBuiltinCylinderParams(const toml::value &table, hgl::graph::BuiltinCylinderParams &out);
    inline bool ReadBuiltinHollowCylinderParams(const toml::value &table, hgl::graph::BuiltinHollowCylinderParams &out);
    inline bool ReadBuiltinTorusParams(const toml::value &table, hgl::graph::BuiltinTorusParams &out);
    inline bool ReadBuiltinConeParams(const toml::value &table, hgl::graph::BuiltinConeParams &out);
    inline bool ReadBuiltinCapsuleParams(const toml::value &table, hgl::graph::BuiltinCapsuleParams &out);
    inline bool ReadBuiltinRectangleParams(const toml::value &table, hgl::graph::BuiltinRectangleParams &out);
    inline bool ReadBuiltinBoundingBoxParams(const toml::value &table, hgl::graph::BuiltinBoundingBoxParams &out);
    inline bool ReadBuiltinHexSphereParams(const toml::value &table, hgl::graph::BuiltinHexSphereParams &out);
    inline bool ReadBuiltinTaperedCapsuleParams(const toml::value &table, hgl::graph::BuiltinTaperedCapsuleParams &out);
    inline bool ReadBuiltinFrustumParams(const toml::value &table, hgl::graph::BuiltinFrustumParams &out);
    inline toml::value WriteBuiltinSphereParams(const hgl::graph::BuiltinSphereParams &in);
    inline toml::value WriteBuiltinDomeParams(const hgl::graph::BuiltinDomeParams &in);
    inline toml::value WriteBuiltinCubeParams(const hgl::graph::BuiltinCubeParams &in);
    inline toml::value WriteBuiltinPlaneGridParams(const hgl::graph::BuiltinPlaneGridParams &in);
    inline toml::value WriteBuiltinCylinderParams(const hgl::graph::BuiltinCylinderParams &in);
    inline toml::value WriteBuiltinHollowCylinderParams(const hgl::graph::BuiltinHollowCylinderParams &in);
    inline toml::value WriteBuiltinTorusParams(const hgl::graph::BuiltinTorusParams &in);
    inline toml::value WriteBuiltinConeParams(const hgl::graph::BuiltinConeParams &in);
    inline toml::value WriteBuiltinCapsuleParams(const hgl::graph::BuiltinCapsuleParams &in);
    inline toml::value WriteBuiltinRectangleParams(const hgl::graph::BuiltinRectangleParams &in);
    inline toml::value WriteBuiltinBoundingBoxParams(const hgl::graph::BuiltinBoundingBoxParams &in);
    inline toml::value WriteBuiltinHexSphereParams(const hgl::graph::BuiltinHexSphereParams &in);
    inline toml::value WriteBuiltinTaperedCapsuleParams(const hgl::graph::BuiltinTaperedCapsuleParams &in);
    inline toml::value WriteBuiltinFrustumParams(const hgl::graph::BuiltinFrustumParams &in);

    inline bool ReadBuiltinSphereParams(const toml::value &table, hgl::graph::BuiltinSphereParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "slices"))
        {
            if (!detail::AsUnsigned(*v, out.slices)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinDomeParams(const toml::value &table, hgl::graph::BuiltinDomeParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "slices"))
        {
            if (!detail::AsUnsigned(*v, out.slices)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinCubeParams(const toml::value &table, hgl::graph::BuiltinCubeParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "segments_x"))
        {
            if (!detail::AsUnsigned(*v, out.segments_x)) return false;
        }
        if (const toml::value *v = detail::Find(table, "segments_y"))
        {
            if (!detail::AsUnsigned(*v, out.segments_y)) return false;
        }
        if (const toml::value *v = detail::Find(table, "segments_z"))
        {
            if (!detail::AsUnsigned(*v, out.segments_z)) return false;
        }
        if (const toml::value *v = detail::Find(table, "ntb"))
        {
            if ((*v).is_string())
            {
                if (!AssetEnumParse_NTBType((*v).as_string().c_str(), out.ntb)) return false;
            }
            else if ((*v).is_integer())
            {
                out.ntb = static_cast<hgl::graph::NTBType>((*v).as_integer());
            }
            else
                return false;
        }
        if (const toml::value *v = detail::Find(table, "tex_coord"))
        {
            if (!detail::AsBool(*v, out.tex_coord)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinPlaneGridParams(const toml::value &table, hgl::graph::BuiltinPlaneGridParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "grid_x"))
        {
            if (!detail::AsUnsigned(*v, out.grid_x)) return false;
        }
        if (const toml::value *v = detail::Find(table, "grid_y"))
        {
            if (!detail::AsUnsigned(*v, out.grid_y)) return false;
        }
        if (const toml::value *v = detail::Find(table, "sub_x"))
        {
            if (!detail::AsUnsigned(*v, out.sub_x)) return false;
        }
        if (const toml::value *v = detail::Find(table, "sub_y"))
        {
            if (!detail::AsUnsigned(*v, out.sub_y)) return false;
        }
        if (const toml::value *v = detail::Find(table, "lum"))
        {
            if (!detail::AsUnsigned(*v, out.lum)) return false;
        }
        if (const toml::value *v = detail::Find(table, "sub_lum"))
        {
            if (!detail::AsUnsigned(*v, out.sub_lum)) return false;
        }
        if (const toml::value *v = detail::Find(table, "is_3d"))
        {
            if (!detail::AsBool(*v, out.is_3d)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinCylinderParams(const toml::value &table, hgl::graph::BuiltinCylinderParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "half_extend"))
        {
            if (!detail::AsFloat(*v, out.half_extend)) return false;
        }
        if (const toml::value *v = detail::Find(table, "radius"))
        {
            if (!detail::AsFloat(*v, out.radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "slices"))
        {
            if (!detail::AsUnsigned(*v, out.slices)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinHollowCylinderParams(const toml::value &table, hgl::graph::BuiltinHollowCylinderParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "half_extend"))
        {
            if (!detail::AsFloat(*v, out.half_extend)) return false;
        }
        if (const toml::value *v = detail::Find(table, "inner_radius"))
        {
            if (!detail::AsFloat(*v, out.inner_radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "outer_radius"))
        {
            if (!detail::AsFloat(*v, out.outer_radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "slices"))
        {
            if (!detail::AsUnsigned(*v, out.slices)) return false;
        }
        if (const toml::value *v = detail::Find(table, "cap_radial_tiles"))
        {
            if (!detail::AsFloat(*v, out.cap_radial_tiles)) return false;
        }
        if (const toml::value *v = detail::Find(table, "cap_angular_tiles"))
        {
            if (!detail::AsFloat(*v, out.cap_angular_tiles)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinTorusParams(const toml::value &table, hgl::graph::BuiltinTorusParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "inner_radius"))
        {
            if (!detail::AsFloat(*v, out.inner_radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "outer_radius"))
        {
            if (!detail::AsFloat(*v, out.outer_radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "slices"))
        {
            if (!detail::AsUnsigned(*v, out.slices)) return false;
        }
        if (const toml::value *v = detail::Find(table, "stacks"))
        {
            if (!detail::AsUnsigned(*v, out.stacks)) return false;
        }
        if (const toml::value *v = detail::Find(table, "uv_scale_x"))
        {
            if (!detail::AsFloat(*v, out.uv_scale_x)) return false;
        }
        if (const toml::value *v = detail::Find(table, "uv_scale_y"))
        {
            if (!detail::AsFloat(*v, out.uv_scale_y)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinConeParams(const toml::value &table, hgl::graph::BuiltinConeParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "half_extend"))
        {
            if (!detail::AsFloat(*v, out.half_extend)) return false;
        }
        if (const toml::value *v = detail::Find(table, "radius"))
        {
            if (!detail::AsFloat(*v, out.radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "slices"))
        {
            if (!detail::AsUnsigned(*v, out.slices)) return false;
        }
        if (const toml::value *v = detail::Find(table, "stacks"))
        {
            if (!detail::AsUnsigned(*v, out.stacks)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinCapsuleParams(const toml::value &table, hgl::graph::BuiltinCapsuleParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "half_height"))
        {
            if (!detail::AsFloat(*v, out.half_height)) return false;
        }
        if (const toml::value *v = detail::Find(table, "radius"))
        {
            if (!detail::AsFloat(*v, out.radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "slices"))
        {
            if (!detail::AsUnsigned(*v, out.slices)) return false;
        }
        if (const toml::value *v = detail::Find(table, "stacks"))
        {
            if (!detail::AsUnsigned(*v, out.stacks)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinRectangleParams(const toml::value &table, hgl::graph::BuiltinRectangleParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "left"))
        {
            if (!detail::AsFloat(*v, out.left)) return false;
        }
        if (const toml::value *v = detail::Find(table, "top"))
        {
            if (!detail::AsFloat(*v, out.top)) return false;
        }
        if (const toml::value *v = detail::Find(table, "width"))
        {
            if (!detail::AsFloat(*v, out.width)) return false;
        }
        if (const toml::value *v = detail::Find(table, "height"))
        {
            if (!detail::AsFloat(*v, out.height)) return false;
        }
        if (const toml::value *v = detail::Find(table, "segments_x"))
        {
            if (!detail::AsUnsigned(*v, out.segments_x)) return false;
        }
        if (const toml::value *v = detail::Find(table, "segments_y"))
        {
            if (!detail::AsUnsigned(*v, out.segments_y)) return false;
        }
        if (const toml::value *v = detail::Find(table, "ntb"))
        {
            if ((*v).is_string())
            {
                if (!AssetEnumParse_NTBType((*v).as_string().c_str(), out.ntb)) return false;
            }
            else if ((*v).is_integer())
            {
                out.ntb = static_cast<hgl::graph::NTBType>((*v).as_integer());
            }
            else
                return false;
        }
        if (const toml::value *v = detail::Find(table, "tex_coord"))
        {
            if (!detail::AsBool(*v, out.tex_coord)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinBoundingBoxParams(const toml::value &table, hgl::graph::BuiltinBoundingBoxParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "ntb"))
        {
            if ((*v).is_string())
            {
                if (!AssetEnumParse_NTBType((*v).as_string().c_str(), out.ntb)) return false;
            }
            else if ((*v).is_integer())
            {
                out.ntb = static_cast<hgl::graph::NTBType>((*v).as_integer());
            }
            else
                return false;
        }
        return true;
    }

    inline bool ReadBuiltinHexSphereParams(const toml::value &table, hgl::graph::BuiltinHexSphereParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "subdivisions"))
        {
            if (!detail::AsUnsigned(*v, out.subdivisions)) return false;
        }
        if (const toml::value *v = detail::Find(table, "radius"))
        {
            if (!detail::AsFloat(*v, out.radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "uv_scale_x"))
        {
            if (!detail::AsFloat(*v, out.uv_scale_x)) return false;
        }
        if (const toml::value *v = detail::Find(table, "uv_scale_y"))
        {
            if (!detail::AsFloat(*v, out.uv_scale_y)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinTaperedCapsuleParams(const toml::value &table, hgl::graph::BuiltinTaperedCapsuleParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "half_height"))
        {
            if (!detail::AsFloat(*v, out.half_height)) return false;
        }
        if (const toml::value *v = detail::Find(table, "bottom_radius"))
        {
            if (!detail::AsFloat(*v, out.bottom_radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "top_radius"))
        {
            if (!detail::AsFloat(*v, out.top_radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "slices"))
        {
            if (!detail::AsUnsigned(*v, out.slices)) return false;
        }
        if (const toml::value *v = detail::Find(table, "stacks"))
        {
            if (!detail::AsUnsigned(*v, out.stacks)) return false;
        }
        return true;
    }

    inline bool ReadBuiltinFrustumParams(const toml::value &table, hgl::graph::BuiltinFrustumParams &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "bottom_radius"))
        {
            if (!detail::AsFloat(*v, out.bottom_radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "top_radius"))
        {
            if (!detail::AsFloat(*v, out.top_radius)) return false;
        }
        if (const toml::value *v = detail::Find(table, "height"))
        {
            if (!detail::AsFloat(*v, out.height)) return false;
        }
        if (const toml::value *v = detail::Find(table, "slices"))
        {
            if (!detail::AsUnsigned(*v, out.slices)) return false;
        }
        if (const toml::value *v = detail::Find(table, "generate_caps"))
        {
            if (!detail::AsBool(*v, out.generate_caps)) return false;
        }
        return true;
    }

    inline toml::value WriteBuiltinSphereParams(const hgl::graph::BuiltinSphereParams &in)
    {
        toml::table t;
        t["slices"] = detail::FromInteger(in.slices);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinDomeParams(const hgl::graph::BuiltinDomeParams &in)
    {
        toml::table t;
        t["slices"] = detail::FromInteger(in.slices);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinCubeParams(const hgl::graph::BuiltinCubeParams &in)
    {
        toml::table t;
        t["segments_x"] = detail::FromInteger(in.segments_x);
        t["segments_y"] = detail::FromInteger(in.segments_y);
        t["segments_z"] = detail::FromInteger(in.segments_z);
        {
            const char *nm = AssetEnumName_NTBType(in.ntb);
            t["ntb"] = nm
                ? toml::value(toml::value::string_type(nm))
                : detail::FromInteger((int64_t)in.ntb);
        }
        t["tex_coord"] = detail::FromBool(in.tex_coord);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinPlaneGridParams(const hgl::graph::BuiltinPlaneGridParams &in)
    {
        toml::table t;
        t["grid_x"] = detail::FromInteger(in.grid_x);
        t["grid_y"] = detail::FromInteger(in.grid_y);
        t["sub_x"] = detail::FromInteger(in.sub_x);
        t["sub_y"] = detail::FromInteger(in.sub_y);
        t["lum"] = detail::FromInteger(in.lum);
        t["sub_lum"] = detail::FromInteger(in.sub_lum);
        t["is_3d"] = detail::FromBool(in.is_3d);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinCylinderParams(const hgl::graph::BuiltinCylinderParams &in)
    {
        toml::table t;
        t["half_extend"] = detail::FromFloat(in.half_extend);
        t["radius"] = detail::FromFloat(in.radius);
        t["slices"] = detail::FromInteger(in.slices);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinHollowCylinderParams(const hgl::graph::BuiltinHollowCylinderParams &in)
    {
        toml::table t;
        t["half_extend"] = detail::FromFloat(in.half_extend);
        t["inner_radius"] = detail::FromFloat(in.inner_radius);
        t["outer_radius"] = detail::FromFloat(in.outer_radius);
        t["slices"] = detail::FromInteger(in.slices);
        t["cap_radial_tiles"] = detail::FromFloat(in.cap_radial_tiles);
        t["cap_angular_tiles"] = detail::FromFloat(in.cap_angular_tiles);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinTorusParams(const hgl::graph::BuiltinTorusParams &in)
    {
        toml::table t;
        t["inner_radius"] = detail::FromFloat(in.inner_radius);
        t["outer_radius"] = detail::FromFloat(in.outer_radius);
        t["slices"] = detail::FromInteger(in.slices);
        t["stacks"] = detail::FromInteger(in.stacks);
        t["uv_scale_x"] = detail::FromFloat(in.uv_scale_x);
        t["uv_scale_y"] = detail::FromFloat(in.uv_scale_y);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinConeParams(const hgl::graph::BuiltinConeParams &in)
    {
        toml::table t;
        t["half_extend"] = detail::FromFloat(in.half_extend);
        t["radius"] = detail::FromFloat(in.radius);
        t["slices"] = detail::FromInteger(in.slices);
        t["stacks"] = detail::FromInteger(in.stacks);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinCapsuleParams(const hgl::graph::BuiltinCapsuleParams &in)
    {
        toml::table t;
        t["half_height"] = detail::FromFloat(in.half_height);
        t["radius"] = detail::FromFloat(in.radius);
        t["slices"] = detail::FromInteger(in.slices);
        t["stacks"] = detail::FromInteger(in.stacks);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinRectangleParams(const hgl::graph::BuiltinRectangleParams &in)
    {
        toml::table t;
        t["left"] = detail::FromFloat(in.left);
        t["top"] = detail::FromFloat(in.top);
        t["width"] = detail::FromFloat(in.width);
        t["height"] = detail::FromFloat(in.height);
        t["segments_x"] = detail::FromInteger(in.segments_x);
        t["segments_y"] = detail::FromInteger(in.segments_y);
        {
            const char *nm = AssetEnumName_NTBType(in.ntb);
            t["ntb"] = nm
                ? toml::value(toml::value::string_type(nm))
                : detail::FromInteger((int64_t)in.ntb);
        }
        t["tex_coord"] = detail::FromBool(in.tex_coord);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinBoundingBoxParams(const hgl::graph::BuiltinBoundingBoxParams &in)
    {
        toml::table t;
        {
            const char *nm = AssetEnumName_NTBType(in.ntb);
            t["ntb"] = nm
                ? toml::value(toml::value::string_type(nm))
                : detail::FromInteger((int64_t)in.ntb);
        }
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinHexSphereParams(const hgl::graph::BuiltinHexSphereParams &in)
    {
        toml::table t;
        t["subdivisions"] = detail::FromInteger(in.subdivisions);
        t["radius"] = detail::FromFloat(in.radius);
        t["uv_scale_x"] = detail::FromFloat(in.uv_scale_x);
        t["uv_scale_y"] = detail::FromFloat(in.uv_scale_y);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinTaperedCapsuleParams(const hgl::graph::BuiltinTaperedCapsuleParams &in)
    {
        toml::table t;
        t["half_height"] = detail::FromFloat(in.half_height);
        t["bottom_radius"] = detail::FromFloat(in.bottom_radius);
        t["top_radius"] = detail::FromFloat(in.top_radius);
        t["slices"] = detail::FromInteger(in.slices);
        t["stacks"] = detail::FromInteger(in.stacks);
        return toml::value(std::move(t));
    }

    inline toml::value WriteBuiltinFrustumParams(const hgl::graph::BuiltinFrustumParams &in)
    {
        toml::table t;
        t["bottom_radius"] = detail::FromFloat(in.bottom_radius);
        t["top_radius"] = detail::FromFloat(in.top_radius);
        t["height"] = detail::FromFloat(in.height);
        t["slices"] = detail::FromInteger(in.slices);
        t["generate_caps"] = detail::FromBool(in.generate_caps);
        return toml::value(std::move(t));
    }

} // namespace hgl::asset::generated
