#pragma once

#include <hgl/mtl/GlslModule.h>
#include <hgl/type/String.h>
#include <hgl/type/StringList.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    /**
     * Parse result of a GLSL code-module file header metadata block.
     *
     * Metadata lives in a `// @ulre begin ... // @ulre end` comment block near
     * the top of the file. Files without any `// @ulre` line are skipped.
     */
    enum class GlslModuleParseResult : uint8
    {
        Skipped = 0,            // no @ulre metadata found; not a module file
        OK,
        MissingBegin,           // @ulre directive seen before begin
        DuplicateBegin,         // second begin inside an open block
        MissingEnd,             // block still open at EOF
        UnknownDirective,       // unrecognized directive keyword
        DuplicateDirective,     // a directive allowed only once appeared twice
        MissingDirectiveArgument,
        InvalidKind,
        InvalidSemantic,
        InvalidSource,
        InvalidNumericClass,
        InvalidNumber,
        InvalidResource,
        InvalidStage,
        InvalidSlotRole,
        InvalidCapability,
        InvalidDependency
    };

    const char *GetGlslModuleParseResultName(GlslModuleParseResult result) noexcept;

    /**
     * Parsed data for one file-backed GLSL code module.
     *
     * The embedded GlslModuleDefinition points into the sibling members
     * (name / glsl_code / semantic arrays), so an instance must be stored at a
     * stable address (e.g. inside a ManagedArray) and its string/array members
     * must not be mutated after the definition pointers are taken.
     */
    struct GlslModuleFileData
    {
        AnsiString name;
        AnsiString glsl_code;

        // Loader provenance; physical paths are not shader identities.
        OSString source_root;
        OSString source_path;
        AnsiString include_path;

        // `uses <module-name>` references; validated against the registry
        // by pass 2 after all files are registered. Names are the identity.
        AnsiStringList pending_module_requirements;

        ValueArray<GlslModuleSemanticRequirement> semantic_requirements;
        ValueArray<GlslModuleSemantic> semantic_provides;
        ValueArray<GlslModuleTextureReferenceRequirement>
            texture_reference_requirements;
        ManagedArray<AnsiString> texture_reference_name_storage;
        ValueArray<GlslModuleSamplerReferenceRequirement>
            sampler_reference_requirements;
        ManagedArray<AnsiString> sampler_reference_name_storage;

        GlslModuleKind kind = GlslModuleKind::Shared;
        int32 priority = 0;
        uint32 flags = 0;
        ShaderModuleSlotRole slot_role = ShaderModuleSlotRole::Unknown;
        uint32 provided_capabilities = 0;
        uint32 required_capabilities = 0;
        bool metadata_resolution_valid = false;
        ValueArray<GlslModuleDependency> dependencies;

        GlslModuleDefinition definition;
    };

    /**
     * Parse the `// @ulre begin/end` metadata block from a GLSL file.
     *
     * @param content      NUL-terminated file content.
     * @param content_size Byte count of content (excluding trailing NUL).
     * @param out_data     Receives parsed name/kind/priority/flags,
     *                     semantic/resource requirements, including
     *                     `texture_reference <name> <stage> [policy]`,
     *                     `sampler_reference <preset-name> [policy]`, and
     *                     `uses` list.
     *                     glsl_code is NOT assigned here; the caller owns the
     *                     content and should copy it into out_data.glsl_code.
     */
    GlslModuleParseResult ParseGlslModuleFile(const char *content,
                                                      int content_size,
                                                      GlslModuleFileData &out_data) noexcept;

    /**
     * 收集 GLSL 正文里所有 `#include "..."` / `#include <...>` 目标。
     *
     * 只按书写原样收集（不做路径归一、不判存在性）；注释行与行尾注释不会被
     * 误收，因为 `#include` 必须位于行首（前导空白除外）。
     *
     * @param content      GLSL 源正文。
     * @param content_size 正文字节数（不含结尾 NUL）。
     * @param out_paths    追加写入的目标路径列表（返回本次新增条数）。
     * @return 本次新增的 include 目标数量。
     */
    int CollectGlslModuleIncludePaths(const char *content,
                                           int content_size,
                                           AnsiStringList &out_paths) noexcept;

}
