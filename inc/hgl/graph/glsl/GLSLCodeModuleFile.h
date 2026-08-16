#pragma once

#include <hgl/graph/glsl/GLSLCodeModule.h>
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
    enum class GLSLCodeModuleParseResult : uint8
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
        MissingMetadataVersion,
        UnsupportedMetadataVersion,
        InvalidCondition,
        InvalidDependency,
        InvalidConflict
    };

    const char *GetGLSLCodeModuleParseResultName(GLSLCodeModuleParseResult result) noexcept;

    /**
     * Parsed data for one file-backed GLSL code module.
     *
     * The embedded GLSLCodeModuleDefinition points into the sibling members
     * (name / glsl_code / semantic arrays), so an instance must be stored at a
     * stable address (e.g. inside a ManagedArray) and its string/array members
     * must not be mutated after the definition pointers are taken.
     */
    struct GLSLCodeModuleFileData
    {
        AnsiString name;
        AnsiString glsl_code;

        // `uses <module-name>` references; validated against the registry
        // by pass 2 after all files are registered. Names are the identity.
        AnsiStringList pending_module_requirements;
        ValueArray<GLSLCodeModuleDependency> pending_dependency_versions;
        AnsiStringList pending_module_conflicts;

        ValueArray<GLSLCodeModuleSemanticRequirement> semantic_requirements;
        ValueArray<GLSLCodeModuleSemantic> semantic_provides;
        ValueArray<GLSLCodeModuleUBORequirement> ubo_requirements;
        ValueArray<GLSLCodeModuleSSBORequirement> ssbo_requirements;
        ValueArray<GLSLCodeModuleTextureLayerRequirement> texture_layer_requirements;
        ManagedArray<AnsiString> ssbo_name_storage;
        ManagedArray<AnsiString> condition_key_storage;
        ManagedArray<AnsiString> condition_value_storage;

        GLSLCodeModuleKind kind = GLSLCodeModuleKind::Shared;
        int32 priority = 0;
        uint32 flags = 0;
        uint16 metadata_version =
            GLSLCodeModuleUnversionedMetadataVersion;
        bool metadata_resolution_valid = false;
        ValueArray<GLSLCodeModuleDependency> dependencies;
        ValueArray<GLSLCodeModuleCondition> conditions;
        // Resolved conflict target names (pointing at target module names).
        ValueArray<const char *> module_conflict_names;

        GLSLCodeModuleDefinition definition;
    };

    /**
     * Parse the `// @ulre begin/end` metadata block from a GLSL file.
     *
     * @param content      NUL-terminated file content.
     * @param content_size Byte count of content (excluding trailing NUL).
     * @param out_data     Receives parsed name/kind/priority/flags,
     *                     semantic/resource requirements, including
     *                     `texture_layer <slot> <stage> [policy]`, and `uses` list.
     *                     glsl_code is NOT assigned here; the caller owns the
     *                     content and should copy it into out_data.glsl_code.
     */
    GLSLCodeModuleParseResult ParseGLSLCodeModuleFile(const char *content,
                                                      int content_size,
                                                      GLSLCodeModuleFileData &out_data) noexcept;
}
