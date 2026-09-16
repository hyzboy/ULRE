#pragma once

#include <hgl/mtl/ShaderCodeModule.h>
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
    enum class ShaderCodeModuleParseResult : uint8
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
        InvalidDependency,
        InvalidConflict
    };

    const char *GetShaderCodeModuleParseResultName(ShaderCodeModuleParseResult result) noexcept;

    /**
     * Parsed data for one file-backed GLSL code module.
     *
     * The embedded ShaderCodeModuleDefinition points into the sibling members
     * (name / glsl_code / semantic arrays), so an instance must be stored at a
     * stable address (e.g. inside a ManagedArray) and its string/array members
     * must not be mutated after the definition pointers are taken.
     */
    struct ShaderCodeModuleFileData
    {
        AnsiString name;
        AnsiString glsl_code;

        // `uses <module-name>` references; validated against the registry
        // by pass 2 after all files are registered. Names are the identity.
        AnsiStringList pending_module_requirements;
        AnsiStringList pending_module_conflicts;

        ValueArray<ShaderCodeModuleSemanticRequirement> semantic_requirements;
        ValueArray<ShaderCodeModuleSemantic> semantic_provides;
        ValueArray<ShaderCodeModuleTextureReferenceRequirement>
            texture_reference_requirements;
        ManagedArray<AnsiString> texture_reference_name_storage;

        ShaderCodeModuleKind kind = ShaderCodeModuleKind::Shared;
        int32 priority = 0;
        uint32 flags = 0;
        ShaderModuleSlotRole slot_role = ShaderModuleSlotRole::Unknown;
        uint32 provided_capabilities = 0;
        uint32 required_capabilities = 0;
        bool metadata_resolution_valid = false;
        ValueArray<ShaderCodeModuleDependency> dependencies;
        // Resolved conflict target names (pointing at target module names).
        ValueArray<const char *> module_conflict_names;

        ShaderCodeModuleDefinition definition;
    };

    /**
     * Parse the `// @ulre begin/end` metadata block from a GLSL file.
     *
     * @param content      NUL-terminated file content.
     * @param content_size Byte count of content (excluding trailing NUL).
     * @param out_data     Receives parsed name/kind/priority/flags,
     *                     semantic/resource requirements, including
     *                     `texture_reference <name> <stage> [policy]`, and
     *                     `uses` list.
     *                     glsl_code is NOT assigned here; the caller owns the
     *                     content and should copy it into out_data.glsl_code.
     */
    ShaderCodeModuleParseResult ParseShaderCodeModuleFile(const char *content,
                                                      int content_size,
                                                      ShaderCodeModuleFileData &out_data) noexcept;

    /**
     * 从库内 include 路径推导模块注册名（@ulre name）。
     *
     *   "material/pbr_surface_source.glsl" → "pbr_surface_source"
     *
     * 规则：取最后一个目录分隔符后的文件名主干（去掉最后一个 '.' 及其后内容）。
     * 这是材质 TOML 里写 include 路径、注册表按注册名查找之间的唯一桥梁——
     * 任何需要「由路径定位模块」的地方都应复用本函数，不要各自手写子串匹配
     * （此前 MaterialShaderEmitter 用 strstr(path,"texture_source") 反推，
     * 既依赖命名又会误命中名字里含该串的模块）。
     *
     * 注意：结果未必等于注册名（如 ao/identity.glsl 注册为 identity_ao），
     * 调用方应以 FindByName 是否命中为准。
     *
     * @return false 表示输入为空或提取不出主干。
     */
    bool ExtractShaderCodeModuleNameFromIncludePath(
        const char *include_path,
        AnsiString &out_name) noexcept;
}
