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
#include <hgl/utf.h>
#include <hgl/asset/generated/scene_schema_generated.h>
#include <hgl/graph/asset/SceneAssetSpec.h>

namespace hgl::asset::generated
{
    inline flatbuffers::Offset<::hgl::asset::schema::SceneTransformSpec>
    EncodeSceneTransformSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneTransformSpec &in);
    inline flatbuffers::Offset<::hgl::asset::schema::SceneTextureSpec>
    EncodeSceneTextureSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneTextureSpec &in);
    inline flatbuffers::Offset<::hgl::asset::schema::SceneMaterialSpec>
    EncodeSceneMaterialSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneMaterialSpec &in);
    inline flatbuffers::Offset<::hgl::asset::schema::SceneNodeSpec>
    EncodeSceneNodeSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneNodeSpec &in);
    inline flatbuffers::Offset<::hgl::asset::schema::SceneSpec>
    EncodeSceneSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneSpec &in);
    inline void DecodeSceneTransformSpec(const ::hgl::asset::schema::SceneTransformSpec *t, hgl::graph::SceneTransformSpec &out);
    inline void DecodeSceneTextureSpec(const ::hgl::asset::schema::SceneTextureSpec *t, hgl::graph::SceneTextureSpec &out);
    inline void DecodeSceneMaterialSpec(const ::hgl::asset::schema::SceneMaterialSpec *t, hgl::graph::SceneMaterialSpec &out);
    inline void DecodeSceneNodeSpec(const ::hgl::asset::schema::SceneNodeSpec *t, hgl::graph::SceneNodeSpec &out);
    inline void DecodeSceneSpec(const ::hgl::asset::schema::SceneSpec *t, hgl::graph::SceneSpec &out);

    inline flatbuffers::Offset<::hgl::asset::schema::SceneTransformSpec>
    EncodeSceneTransformSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneTransformSpec &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        const ::hgl::asset::schema::Float3 _SceneTransformSpec_position_vec(in.position.x, in.position.y, in.position.z);
        const ::hgl::asset::schema::Float3 _SceneTransformSpec_rotation_axis_vec(in.rotation_axis.x, in.rotation_axis.y, in.rotation_axis.z);
        const ::hgl::asset::schema::Float3 _SceneTransformSpec_scale_vec(in.scale.x, in.scale.y, in.scale.z);
        ::hgl::asset::schema::SceneTransformSpecBuilder b(fbb);
        b.add_position(&_SceneTransformSpec_position_vec);
        b.add_rotation_axis(&_SceneTransformSpec_rotation_axis_vec);
        b.add_rotation_angle_deg(in.rotation_angle_deg);
        b.add_scale(&_SceneTransformSpec_scale_vec);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::SceneTextureSpec>
    EncodeSceneTextureSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneTextureSpec &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        const auto _SceneTextureSpec_name_str_offset = fbb.CreateString(in.name.c_str() ? in.name.c_str() : "");
        const hgl::U8String _SceneTextureSpec_uri_str_u8 = hgl::ToU8String(in.uri);
        const auto _SceneTextureSpec_uri_str_offset = fbb.CreateString(reinterpret_cast<const char *>(_SceneTextureSpec_uri_str_u8.c_str() ? _SceneTextureSpec_uri_str_u8.c_str() : U8_TEXT("")));
        ::hgl::asset::schema::SceneTextureSpecBuilder b(fbb);
        b.add_name(_SceneTextureSpec_name_str_offset);
        b.add_uri(_SceneTextureSpec_uri_str_offset);
        b.add_required(in.required);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::SceneMaterialSpec>
    EncodeSceneMaterialSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneMaterialSpec &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        const auto _SceneMaterialSpec_name_str_offset = fbb.CreateString(in.name.c_str() ? in.name.c_str() : "");
        const ::hgl::asset::schema::Float4 _SceneMaterialSpec_base_color_vec(in.base_color.x, in.base_color.y, in.base_color.z, in.base_color.w);
        const ::hgl::asset::schema::Float4 _SceneMaterialSpec_color_vec(in.color.x, in.color.y, in.color.z, in.color.w);
        const ::hgl::asset::schema::Float4 _SceneMaterialSpec_trans_color_vec(in.trans_color.x, in.trans_color.y, in.trans_color.z, in.trans_color.w);
        const auto _SceneMaterialSpec_textures_vec = fbb.CreateVector<flatbuffers::Offset<::hgl::asset::schema::SceneTextureSpec>>((size_t)in.textures.GetCount(), [&](size_t i, const hgl::graph::SceneMaterialSpec *source) { return EncodeSceneTextureSpec(fbb, *source->textures[(int)i]); }, &in);
        ::hgl::asset::schema::SceneMaterialSpecBuilder b(fbb);
        b.add_name(_SceneMaterialSpec_name_str_offset);
        b.add_type(static_cast<int32_t>(in.type));
        b.add_base_color(&_SceneMaterialSpec_base_color_vec);
        b.add_metallic(in.metallic);
        b.add_roughness(in.roughness);
        b.add_normal_scale(in.normal_scale);
        b.add_fresnel(in.fresnel);
        b.add_color(&_SceneMaterialSpec_color_vec);
        b.add_trans_color(&_SceneMaterialSpec_trans_color_vec);
        b.add_textures(_SceneMaterialSpec_textures_vec);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::SceneNodeSpec>
    EncodeSceneNodeSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneNodeSpec &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        const auto _SceneNodeSpec_name_str_offset = fbb.CreateString(in.name.c_str() ? in.name.c_str() : "");
        const auto _SceneNodeSpec_parent_str_offset = fbb.CreateString(in.parent.c_str() ? in.parent.c_str() : "");
        const auto _SceneNodeSpec_geometry_str_offset = fbb.CreateString(in.geometry.c_str() ? in.geometry.c_str() : "");
        const auto _SceneNodeSpec_recipe_str_offset = fbb.CreateString(in.recipe.c_str() ? in.recipe.c_str() : "");
        const auto _SceneNodeSpec_material_str_offset = fbb.CreateString(in.material.c_str() ? in.material.c_str() : "");
        const auto _SceneNodeSpec_mobility_str_offset = fbb.CreateString(in.mobility.c_str() ? in.mobility.c_str() : "");
        const auto _SceneNodeSpec_transform_tab = EncodeSceneTransformSpec(fbb, in.transform);
        ::hgl::asset::schema::SceneNodeSpecBuilder b(fbb);
        b.add_name(_SceneNodeSpec_name_str_offset);
        b.add_parent(_SceneNodeSpec_parent_str_offset);
        b.add_geometry(_SceneNodeSpec_geometry_str_offset);
        b.add_recipe(_SceneNodeSpec_recipe_str_offset);
        b.add_material(_SceneNodeSpec_material_str_offset);
        b.add_mobility(_SceneNodeSpec_mobility_str_offset);
        b.add_visible(in.visible);
        b.add_transform(_SceneNodeSpec_transform_tab);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::SceneSpec>
    EncodeSceneSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::SceneSpec &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        const auto _SceneSpec_name_str_offset = fbb.CreateString(in.name.c_str() ? in.name.c_str() : "");
        const auto _SceneSpec_description_str_offset = fbb.CreateString(in.description.c_str() ? in.description.c_str() : "");
        const auto _SceneSpec_materials_vec = fbb.CreateVector<flatbuffers::Offset<::hgl::asset::schema::SceneMaterialSpec>>((size_t)in.materials.GetCount(), [&](size_t i, const hgl::graph::SceneSpec *source) { return EncodeSceneMaterialSpec(fbb, *source->materials[(int)i]); }, &in);
        const auto _SceneSpec_nodes_vec = fbb.CreateVector<flatbuffers::Offset<::hgl::asset::schema::SceneNodeSpec>>((size_t)in.nodes.GetCount(), [&](size_t i, const hgl::graph::SceneSpec *source) { return EncodeSceneNodeSpec(fbb, *source->nodes[(int)i]); }, &in);
        ::hgl::asset::schema::SceneSpecBuilder b(fbb);
        b.add_schema(in.schema);
        b.add_name(_SceneSpec_name_str_offset);
        b.add_description(_SceneSpec_description_str_offset);
        b.add_materials(_SceneSpec_materials_vec);
        b.add_nodes(_SceneSpec_nodes_vec);
        return b.Finish();
    }

    inline void DecodeSceneTransformSpec(const ::hgl::asset::schema::SceneTransformSpec *t, hgl::graph::SceneTransformSpec &out)
    {
        if (!t) return;
        if (const auto *v = t->position())
        {
            out.position.x = v->x();
            out.position.y = v->y();
            out.position.z = v->z();
        }
        if (const auto *v = t->rotation_axis())
        {
            out.rotation_axis.x = v->x();
            out.rotation_axis.y = v->y();
            out.rotation_axis.z = v->z();
        }
        out.rotation_angle_deg = t->rotation_angle_deg();
        if (const auto *v = t->scale())
        {
            out.scale.x = v->x();
            out.scale.y = v->y();
            out.scale.z = v->z();
        }
    }

    inline void DecodeSceneTextureSpec(const ::hgl::asset::schema::SceneTextureSpec *t, hgl::graph::SceneTextureSpec &out)
    {
        if (!t) return;
        if (const ::flatbuffers::String *s = t->name()) out.name = s->str().c_str();
        if (const ::flatbuffers::String *s = t->uri()) out.uri = hgl::ToOSString(s->str().c_str());
        out.required = t->required();
    }

    inline void DecodeSceneMaterialSpec(const ::hgl::asset::schema::SceneMaterialSpec *t, hgl::graph::SceneMaterialSpec &out)
    {
        if (!t) return;
        if (const ::flatbuffers::String *s = t->name()) out.name = s->str().c_str();
        out.type = static_cast<hgl::graph::SceneMaterialType>(t->type());
        if (const auto *v = t->base_color())
        {
            out.base_color.x = v->x();
            out.base_color.y = v->y();
            out.base_color.z = v->z();
            out.base_color.w = v->w();
        }
        out.metallic = t->metallic();
        out.roughness = t->roughness();
        out.normal_scale = t->normal_scale();
        out.fresnel = t->fresnel();
        if (const auto *v = t->color())
        {
            out.color.x = v->x();
            out.color.y = v->y();
            out.color.z = v->z();
            out.color.w = v->w();
        }
        if (const auto *v = t->trans_color())
        {
            out.trans_color.x = v->x();
            out.trans_color.y = v->y();
            out.trans_color.z = v->z();
            out.trans_color.w = v->w();
        }
        if (const auto *v = t->textures())
        {
            out.textures.Clear();
            for (uint32_t i = 0; i < v->size(); ++i)
            {
                auto *item = out.textures.Create();
                DecodeSceneTextureSpec(v->Get(i), *item);
            }
        }
    }

    inline void DecodeSceneNodeSpec(const ::hgl::asset::schema::SceneNodeSpec *t, hgl::graph::SceneNodeSpec &out)
    {
        if (!t) return;
        if (const ::flatbuffers::String *s = t->name()) out.name = s->str().c_str();
        if (const ::flatbuffers::String *s = t->parent()) out.parent = s->str().c_str();
        if (const ::flatbuffers::String *s = t->geometry()) out.geometry = s->str().c_str();
        if (const ::flatbuffers::String *s = t->recipe()) out.recipe = s->str().c_str();
        if (const ::flatbuffers::String *s = t->material()) out.material = s->str().c_str();
        if (const ::flatbuffers::String *s = t->mobility()) out.mobility = s->str().c_str();
        out.visible = t->visible();
        DecodeSceneTransformSpec(t->transform(), out.transform);
    }

    inline void DecodeSceneSpec(const ::hgl::asset::schema::SceneSpec *t, hgl::graph::SceneSpec &out)
    {
        if (!t) return;
        out.schema = t->schema();
        if (const ::flatbuffers::String *s = t->name()) out.name = s->str().c_str();
        if (const ::flatbuffers::String *s = t->description()) out.description = s->str().c_str();
        if (const auto *v = t->materials())
        {
            out.materials.Clear();
            for (uint32_t i = 0; i < v->size(); ++i)
            {
                auto *item = out.materials.Create();
                DecodeSceneMaterialSpec(v->Get(i), *item);
            }
        }
        if (const auto *v = t->nodes())
        {
            out.nodes.Clear();
            for (uint32_t i = 0; i < v->size(); ++i)
            {
                auto *item = out.nodes.Create();
                DecodeSceneNodeSpec(v->Get(i), *item);
            }
        }
    }

} // namespace hgl::asset::generated
