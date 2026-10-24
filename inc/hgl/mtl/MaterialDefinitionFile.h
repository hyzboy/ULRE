#pragma once

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    enum class MaterialDefinitionFileParseResult : uint8
    {
        Skipped = 0,
        OK,
        InvalidValue,
        InvalidSemantic,
        InvalidNumber,
        UnknownKey
    };

    const char *GetMaterialDefinitionFileParseResultName(
        MaterialDefinitionFileParseResult result) noexcept;

    struct MaterialDefinitionFileData
    {
        MaterialDefinition definition;
        AnsiString material_source_module_name_storage;
        AnsiString ntb_module_name_storage;
        AnsiString surface_module_name_storage;

        // ── 解析失败细节（2026-10-12 用户口径）────────────────────────────
        // 发售口径：全预编译 SPV、届时不再有 shader 合成器 ⇒ 材质配置写错只能靠
        // **日志**定位，所以失败必须报到"哪个表、哪个键、什么值"级别，而不是只说
        // "某文件解析失败"。仅当 ParseMaterialDefinitionFile 返回非 OK 时有意义；
        // 只记**第一个**失败点（根因优先），诊断用、不参与任何控制流判定。
        struct ParseErrorDetail
        {
            std::string table;   // 出错所在表/段落（"root"/"transform"/"render_state.pipeline"/…）
            std::string key;     // 出错键名（如 "sampler"；空 = 未定位到键）
            std::string value;   // 出错值原文（如 "SamplerBogus"；空 = 无值可报）
            std::string detail;  // 人类可读补充（期望提示 / TOML 报错原文）
            int         line = 0; // TOML 语法错误行号（1-based；0 = 不适用）

            bool IsEmpty() const noexcept
            {
                return line == 0 && table.empty() && key.empty()
                    && value.empty() && detail.empty();
            }
        };

        ParseErrorDetail parse_error;
    };

    MaterialDefinitionFileParseResult ParseMaterialDefinitionFile(
        const char *content,
        int content_size,
        MaterialDefinitionFileData &out_data) noexcept;

    bool IsValidMaterialDefinitionFileData(
        const MaterialDefinitionFileData &data) noexcept;

    class MaterialDefinitionFileRegistry
    {
        ManagedArray<MaterialDefinitionFileData> files;

    public:
        bool LoadFile(const OSString &path);
        bool LoadDirectory(const OSString &directory,
                           int *out_file_count = nullptr,
                           int *out_error_count = nullptr);

        /**
         * 注册一份**内嵌**的 schema-4 源码（不碰磁盘）。
         *
         * 与 LoadFile 共用同一套解析 + 领域校验 + 重复 ID 检测：代码化只消灭"对磁盘文件的
         * 依赖"，不消灭"对校验管线的依赖"（领域契约仍是唯一真源）。
         * label 仅用于诊断日志，应形如 OS_TEXT("builtin:pure_color")。
         * 解析失败或 ID 重复 ⇒ 该份不注册并返回 false。
         */
        bool RegisterDefinitionSource(const os_char *label,
                                      const char *content,
                                      int content_size);

        const MaterialDefinition *FindByID(const char *definition_id) const;
        int GetCount() const { return files.GetCount(); }
        void Clear() { files.Clear(); }
    };
}
