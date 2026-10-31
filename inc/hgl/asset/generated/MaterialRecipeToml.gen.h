// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   TOML 格式门面（Read* / Write*）。字段遍历仍然只有 Serialize.gen.h 里那一份，
//   本文件不含任何 FlatBuffers 依赖，数据面可以单独包含它。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/asset/generated/MaterialRecipeSerialize.gen.h>

#include <hgl/graph/asset/MaterialRecipeSpec.h>

namespace hgl::asset::generated
{
    inline bool ReadRecipeTextureSpec(const toml::value &table, hgl::graph::mtl::RecipeTextureSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTextureSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteRecipeTextureSpec(const hgl::graph::mtl::RecipeTextureSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTextureSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadRecipePipelineSpec(const toml::value &table, hgl::graph::mtl::RecipePipelineSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipePipelineSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteRecipePipelineSpec(const hgl::graph::mtl::RecipePipelineSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipePipelineSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadRecipeRenderStateSpec(const toml::value &table, hgl::graph::mtl::RecipeRenderStateSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeRenderStateSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteRecipeRenderStateSpec(const hgl::graph::mtl::RecipeRenderStateSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeRenderStateSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadRecipeFragmentSpec(const toml::value &table, hgl::graph::mtl::RecipeFragmentSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeFragmentSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteRecipeFragmentSpec(const hgl::graph::mtl::RecipeFragmentSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeFragmentSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadRecipeTransformSpec(const toml::value &table, hgl::graph::mtl::RecipeTransformSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTransformSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteRecipeTransformSpec(const hgl::graph::mtl::RecipeTransformSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::RecipeTransformSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadMaterialRecipeSpec(const toml::value &table, hgl::graph::mtl::MaterialRecipeSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::MaterialRecipeSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteMaterialRecipeSpec(const hgl::graph::mtl::MaterialRecipeSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::mtl::MaterialRecipeSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

}//namespace hgl::asset::generated
