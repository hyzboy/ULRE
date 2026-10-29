// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   语义：**完整快照**。与 TOML 的「缺键即保留 C++ 缺省值」不同，这里的 Decode
//         会把 schema 里存在的每个字段都写回，缺省值来自类内初始化（Transient
//         字段不进 schema，因此保持类内缺省）。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <flatbuffers/flatbuffers.h>
#include <hgl/type/ValueArray.h>
#include <hgl/utf.h>
#include <hgl/asset/generated/geometry_schema_generated.h>
#include <hgl/graph/asset/GeometryAssetSpec.h>

namespace hgl::asset::generated
{
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinSphereParams>
    EncodeBuiltinSphereParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinSphereParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinDomeParams>
    EncodeBuiltinDomeParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinDomeParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinCubeParams>
    EncodeBuiltinCubeParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinCubeParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinPlaneGridParams>
    EncodeBuiltinPlaneGridParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinPlaneGridParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinCylinderParams>
    EncodeBuiltinCylinderParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinCylinderParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinHollowCylinderParams>
    EncodeBuiltinHollowCylinderParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinHollowCylinderParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinTorusParams>
    EncodeBuiltinTorusParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinTorusParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinConeParams>
    EncodeBuiltinConeParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinConeParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinCapsuleParams>
    EncodeBuiltinCapsuleParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinCapsuleParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinRectangleParams>
    EncodeBuiltinRectangleParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinRectangleParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinBoundingBoxParams>
    EncodeBuiltinBoundingBoxParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinBoundingBoxParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinHexSphereParams>
    EncodeBuiltinHexSphereParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinHexSphereParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinTaperedCapsuleParams>
    EncodeBuiltinTaperedCapsuleParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinTaperedCapsuleParams &in);
    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinFrustumParams>
    EncodeBuiltinFrustumParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinFrustumParams &in);
    inline void DecodeBuiltinSphereParams(const ::hgl::asset::schema::BuiltinSphereParams *t, hgl::graph::BuiltinSphereParams &out);
    inline void DecodeBuiltinDomeParams(const ::hgl::asset::schema::BuiltinDomeParams *t, hgl::graph::BuiltinDomeParams &out);
    inline void DecodeBuiltinCubeParams(const ::hgl::asset::schema::BuiltinCubeParams *t, hgl::graph::BuiltinCubeParams &out);
    inline void DecodeBuiltinPlaneGridParams(const ::hgl::asset::schema::BuiltinPlaneGridParams *t, hgl::graph::BuiltinPlaneGridParams &out);
    inline void DecodeBuiltinCylinderParams(const ::hgl::asset::schema::BuiltinCylinderParams *t, hgl::graph::BuiltinCylinderParams &out);
    inline void DecodeBuiltinHollowCylinderParams(const ::hgl::asset::schema::BuiltinHollowCylinderParams *t, hgl::graph::BuiltinHollowCylinderParams &out);
    inline void DecodeBuiltinTorusParams(const ::hgl::asset::schema::BuiltinTorusParams *t, hgl::graph::BuiltinTorusParams &out);
    inline void DecodeBuiltinConeParams(const ::hgl::asset::schema::BuiltinConeParams *t, hgl::graph::BuiltinConeParams &out);
    inline void DecodeBuiltinCapsuleParams(const ::hgl::asset::schema::BuiltinCapsuleParams *t, hgl::graph::BuiltinCapsuleParams &out);
    inline void DecodeBuiltinRectangleParams(const ::hgl::asset::schema::BuiltinRectangleParams *t, hgl::graph::BuiltinRectangleParams &out);
    inline void DecodeBuiltinBoundingBoxParams(const ::hgl::asset::schema::BuiltinBoundingBoxParams *t, hgl::graph::BuiltinBoundingBoxParams &out);
    inline void DecodeBuiltinHexSphereParams(const ::hgl::asset::schema::BuiltinHexSphereParams *t, hgl::graph::BuiltinHexSphereParams &out);
    inline void DecodeBuiltinTaperedCapsuleParams(const ::hgl::asset::schema::BuiltinTaperedCapsuleParams *t, hgl::graph::BuiltinTaperedCapsuleParams &out);
    inline void DecodeBuiltinFrustumParams(const ::hgl::asset::schema::BuiltinFrustumParams *t, hgl::graph::BuiltinFrustumParams &out);

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinSphereParams>
    EncodeBuiltinSphereParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinSphereParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinSphereParamsBuilder b(fbb);
        b.add_slices(in.slices);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinDomeParams>
    EncodeBuiltinDomeParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinDomeParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinDomeParamsBuilder b(fbb);
        b.add_slices(in.slices);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinCubeParams>
    EncodeBuiltinCubeParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinCubeParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinCubeParamsBuilder b(fbb);
        b.add_segments_x(in.segments_x);
        b.add_segments_y(in.segments_y);
        b.add_segments_z(in.segments_z);
        b.add_ntb(static_cast<int32_t>(in.ntb));
        b.add_tex_coord(in.tex_coord);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinPlaneGridParams>
    EncodeBuiltinPlaneGridParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinPlaneGridParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinPlaneGridParamsBuilder b(fbb);
        b.add_grid_x(in.grid_x);
        b.add_grid_y(in.grid_y);
        b.add_sub_x(in.sub_x);
        b.add_sub_y(in.sub_y);
        b.add_lum(in.lum);
        b.add_sub_lum(in.sub_lum);
        b.add_is_3d(in.is_3d);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinCylinderParams>
    EncodeBuiltinCylinderParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinCylinderParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinCylinderParamsBuilder b(fbb);
        b.add_half_extend(in.half_extend);
        b.add_radius(in.radius);
        b.add_slices(in.slices);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinHollowCylinderParams>
    EncodeBuiltinHollowCylinderParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinHollowCylinderParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinHollowCylinderParamsBuilder b(fbb);
        b.add_half_extend(in.half_extend);
        b.add_inner_radius(in.inner_radius);
        b.add_outer_radius(in.outer_radius);
        b.add_slices(in.slices);
        b.add_cap_radial_tiles(in.cap_radial_tiles);
        b.add_cap_angular_tiles(in.cap_angular_tiles);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinTorusParams>
    EncodeBuiltinTorusParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinTorusParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinTorusParamsBuilder b(fbb);
        b.add_inner_radius(in.inner_radius);
        b.add_outer_radius(in.outer_radius);
        b.add_slices(in.slices);
        b.add_stacks(in.stacks);
        b.add_uv_scale_x(in.uv_scale_x);
        b.add_uv_scale_y(in.uv_scale_y);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinConeParams>
    EncodeBuiltinConeParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinConeParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinConeParamsBuilder b(fbb);
        b.add_half_extend(in.half_extend);
        b.add_radius(in.radius);
        b.add_slices(in.slices);
        b.add_stacks(in.stacks);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinCapsuleParams>
    EncodeBuiltinCapsuleParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinCapsuleParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinCapsuleParamsBuilder b(fbb);
        b.add_half_height(in.half_height);
        b.add_radius(in.radius);
        b.add_slices(in.slices);
        b.add_stacks(in.stacks);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinRectangleParams>
    EncodeBuiltinRectangleParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinRectangleParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinRectangleParamsBuilder b(fbb);
        b.add_left(in.left);
        b.add_top(in.top);
        b.add_width(in.width);
        b.add_height(in.height);
        b.add_segments_x(in.segments_x);
        b.add_segments_y(in.segments_y);
        b.add_ntb(static_cast<int32_t>(in.ntb));
        b.add_tex_coord(in.tex_coord);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinBoundingBoxParams>
    EncodeBuiltinBoundingBoxParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinBoundingBoxParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinBoundingBoxParamsBuilder b(fbb);
        b.add_ntb(static_cast<int32_t>(in.ntb));
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinHexSphereParams>
    EncodeBuiltinHexSphereParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinHexSphereParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinHexSphereParamsBuilder b(fbb);
        b.add_subdivisions(in.subdivisions);
        b.add_radius(in.radius);
        b.add_uv_scale_x(in.uv_scale_x);
        b.add_uv_scale_y(in.uv_scale_y);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinTaperedCapsuleParams>
    EncodeBuiltinTaperedCapsuleParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinTaperedCapsuleParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinTaperedCapsuleParamsBuilder b(fbb);
        b.add_half_height(in.half_height);
        b.add_bottom_radius(in.bottom_radius);
        b.add_top_radius(in.top_radius);
        b.add_slices(in.slices);
        b.add_stacks(in.stacks);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinFrustumParams>
    EncodeBuiltinFrustumParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinFrustumParams &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        ::hgl::asset::schema::BuiltinFrustumParamsBuilder b(fbb);
        b.add_bottom_radius(in.bottom_radius);
        b.add_top_radius(in.top_radius);
        b.add_height(in.height);
        b.add_slices(in.slices);
        b.add_generate_caps(in.generate_caps);
        return b.Finish();
    }

    inline void DecodeBuiltinSphereParams(const ::hgl::asset::schema::BuiltinSphereParams *t, hgl::graph::BuiltinSphereParams &out)
    {
        if (!t) return;
        out.slices = t->slices();
    }

    inline void DecodeBuiltinDomeParams(const ::hgl::asset::schema::BuiltinDomeParams *t, hgl::graph::BuiltinDomeParams &out)
    {
        if (!t) return;
        out.slices = t->slices();
    }

    inline void DecodeBuiltinCubeParams(const ::hgl::asset::schema::BuiltinCubeParams *t, hgl::graph::BuiltinCubeParams &out)
    {
        if (!t) return;
        out.segments_x = t->segments_x();
        out.segments_y = t->segments_y();
        out.segments_z = t->segments_z();
        out.ntb = static_cast<hgl::graph::NTBType>(t->ntb());
        out.tex_coord = t->tex_coord();
    }

    inline void DecodeBuiltinPlaneGridParams(const ::hgl::asset::schema::BuiltinPlaneGridParams *t, hgl::graph::BuiltinPlaneGridParams &out)
    {
        if (!t) return;
        out.grid_x = t->grid_x();
        out.grid_y = t->grid_y();
        out.sub_x = t->sub_x();
        out.sub_y = t->sub_y();
        out.lum = t->lum();
        out.sub_lum = t->sub_lum();
        out.is_3d = t->is_3d();
    }

    inline void DecodeBuiltinCylinderParams(const ::hgl::asset::schema::BuiltinCylinderParams *t, hgl::graph::BuiltinCylinderParams &out)
    {
        if (!t) return;
        out.half_extend = t->half_extend();
        out.radius = t->radius();
        out.slices = t->slices();
    }

    inline void DecodeBuiltinHollowCylinderParams(const ::hgl::asset::schema::BuiltinHollowCylinderParams *t, hgl::graph::BuiltinHollowCylinderParams &out)
    {
        if (!t) return;
        out.half_extend = t->half_extend();
        out.inner_radius = t->inner_radius();
        out.outer_radius = t->outer_radius();
        out.slices = t->slices();
        out.cap_radial_tiles = t->cap_radial_tiles();
        out.cap_angular_tiles = t->cap_angular_tiles();
    }

    inline void DecodeBuiltinTorusParams(const ::hgl::asset::schema::BuiltinTorusParams *t, hgl::graph::BuiltinTorusParams &out)
    {
        if (!t) return;
        out.inner_radius = t->inner_radius();
        out.outer_radius = t->outer_radius();
        out.slices = t->slices();
        out.stacks = t->stacks();
        out.uv_scale_x = t->uv_scale_x();
        out.uv_scale_y = t->uv_scale_y();
    }

    inline void DecodeBuiltinConeParams(const ::hgl::asset::schema::BuiltinConeParams *t, hgl::graph::BuiltinConeParams &out)
    {
        if (!t) return;
        out.half_extend = t->half_extend();
        out.radius = t->radius();
        out.slices = t->slices();
        out.stacks = t->stacks();
    }

    inline void DecodeBuiltinCapsuleParams(const ::hgl::asset::schema::BuiltinCapsuleParams *t, hgl::graph::BuiltinCapsuleParams &out)
    {
        if (!t) return;
        out.half_height = t->half_height();
        out.radius = t->radius();
        out.slices = t->slices();
        out.stacks = t->stacks();
    }

    inline void DecodeBuiltinRectangleParams(const ::hgl::asset::schema::BuiltinRectangleParams *t, hgl::graph::BuiltinRectangleParams &out)
    {
        if (!t) return;
        out.left = t->left();
        out.top = t->top();
        out.width = t->width();
        out.height = t->height();
        out.segments_x = t->segments_x();
        out.segments_y = t->segments_y();
        out.ntb = static_cast<hgl::graph::NTBType>(t->ntb());
        out.tex_coord = t->tex_coord();
    }

    inline void DecodeBuiltinBoundingBoxParams(const ::hgl::asset::schema::BuiltinBoundingBoxParams *t, hgl::graph::BuiltinBoundingBoxParams &out)
    {
        if (!t) return;
        out.ntb = static_cast<hgl::graph::NTBType>(t->ntb());
    }

    inline void DecodeBuiltinHexSphereParams(const ::hgl::asset::schema::BuiltinHexSphereParams *t, hgl::graph::BuiltinHexSphereParams &out)
    {
        if (!t) return;
        out.subdivisions = t->subdivisions();
        out.radius = t->radius();
        out.uv_scale_x = t->uv_scale_x();
        out.uv_scale_y = t->uv_scale_y();
    }

    inline void DecodeBuiltinTaperedCapsuleParams(const ::hgl::asset::schema::BuiltinTaperedCapsuleParams *t, hgl::graph::BuiltinTaperedCapsuleParams &out)
    {
        if (!t) return;
        out.half_height = t->half_height();
        out.bottom_radius = t->bottom_radius();
        out.top_radius = t->top_radius();
        out.slices = t->slices();
        out.stacks = t->stacks();
    }

    inline void DecodeBuiltinFrustumParams(const ::hgl::asset::schema::BuiltinFrustumParams *t, hgl::graph::BuiltinFrustumParams &out)
    {
        if (!t) return;
        out.bottom_radius = t->bottom_radius();
        out.top_radius = t->top_radius();
        out.height = t->height();
        out.slices = t->slices();
        out.generate_caps = t->generate_caps();
    }

} // namespace hgl::asset::generated
