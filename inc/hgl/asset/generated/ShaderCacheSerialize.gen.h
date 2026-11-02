// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，
//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/mtl/ShaderArtifactContract.h>
#include <hgl/mtl/ShaderComputeProgramManifest.h>
#include <hgl/mtl/ShaderLinkSpec.h>
#include <hgl/mtl/ShaderProgramManifest.h>
#include <hgl/mtl/ShaderStageKey.h>
#include <hgl/asset/generated/ShaderCacheMeta.gen.h>

namespace hgl::serialize
{
    template<> struct StructSerializer<hgl::graph::mtl::ShaderStageKey>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "stage", v.stage);
            ar.Field(2, "definition_hash", v.definition_hash);
            ar.Field(3, "compile_inputs_hash", v.compile_inputs_hash);
            ar.Field(4, "interface_hash", v.interface_hash);
            ar.Field(5, "compiler_hash", v.compiler_hash);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_ShaderStageKey; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::ShaderLinkSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "mesh_stage", v.mesh_stage);
            ar.Field(2, "fragment_stage", v.fragment_stage);
            ar.Field(3, "link_contract_hash", v.link_contract_hash);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_ShaderLinkSpec; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::ShaderProgramArtifactMetadata>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "program_key_digest", v.program_key_digest);
            ar.Field(2, "generated_source_digest", v.generated_source_digest);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_ShaderProgramArtifactMetadata; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::ShaderProgramManifestEntry>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "lookup_key", v.lookup_key);
            ar.Field(2, "program_link", v.program_link);
            ar.Field(3, "requires_recipe_runtime_rows", v.requires_recipe_runtime_rows);
            ar.Field(4, "requires_recipe_payload_row", v.requires_recipe_payload_row);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_ShaderProgramManifestEntry; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::ShaderProgramManifestPayload>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "format_version", v.format_version);
            ar.Field(2, "entries", v.entries);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_ShaderProgramManifestPayload; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::ShaderComputeProgramManifestEntry>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "program_key", v.program_key);
            ar.Field(2, "compute_stage", v.compute_stage);
            ar.Field(3, "push_constant_size", v.push_constant_size);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_ShaderComputeProgramManifestEntry; }
    };

    template<> struct StructSerializer<hgl::graph::mtl::ShaderComputeProgramManifestPayload>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "format_version", v.format_version);
            ar.Field(2, "entries", v.entries);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_ShaderComputeProgramManifestPayload; }
    };

}//namespace hgl::serialize
