#pragma once

/**
 * 材质数据块的文件层：TOML 授权 → 运行期 payload，以及 `MDB1` 二进制直通形态。
 *
 * 与 `MaterialRecipeFile.h` 的分工完全一样：
 *
 * · 字段抽取全部交给生成的 `StructSerializer<MaterialDataBlockSpec>`；
 * · 本层只做**跨字段**校验（schema 版本、target 必须是内容行类型、本 target
 *   用不到的键必须报错、行数上限），以及**授权形态 → 物理形态**的摊平。
 *
 * ── `MDB1` 二进制形态 ──────────────────────────────────────────────────────
 *
 * ```
 * 偏移 0   u8[4]  'M','D','B','1'
 * 偏移 4   u16    version (1)
 * 偏移 6   u8     row_type (GlobalSSBOType)
 * 偏移 7   u8     reserved (0)
 * 偏移 8   u32    row_bytes
 * 偏移 12  u32    row_count
 * 偏移 16  ...    row_bytes * row_count 行数据（与 GPU 行逐字节相同）
 * ```
 *
 * 没有 vtable、没有根偏移、没有对齐补齐：`ViewMaterialDataBlockBinary` 只校验
 * 16 字节头，把 `rows` 指向文件缓冲即可直接 `memcpy` 到 mapped 指针。这是
 * 「FlatBuffers struct 数组可直通 GPU」的落地方式 —— 既然不需要 vtable，
 * 就不必为它引入 `.fbs` 与 flatc（生成器、schema 漂移、对齐断言都是纯成本）。
 *
 * 需要 FlatBuffers 形态的**工具**（检查器、编辑器）走通用 `hgl::serialize`
 * 的 FlatBuffers 后端序列化 `MaterialDataBlockSpec`，与这里互不干扰。
 */

#include <hgl/graph/asset/MaterialDataBlockSpec.h>
#include <hgl/mtl/MaterialDataBlock.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    enum class MaterialDataBlockParseResult : uint8
    {
        Skipped = 0,    ///< 空内容（调用方按「没有文件」处理）
        OK,
        InvalidValue,   ///< 语法/取值/跨字段校验失败
        UnknownKey      ///< 键名不在 schema 里（或不属于本 target 的行类型）
    };

    struct MaterialDataBlockParseError
    {
        AnsiString table;
        AnsiString key;
        AnsiString value;
        AnsiString detail;
        int line = 0;

        bool IsEmpty() const noexcept
        {
            return line == 0 && table.IsEmpty() && key.IsEmpty()
                && value.IsEmpty() && detail.IsEmpty();
        }
    };

    struct MaterialDataBlockFileData
    {
        MaterialDataBlock            block;
        MaterialDataBlockParseError  parse_error;
    };

    const char *GetMaterialDataBlockParseResultName(
        MaterialDataBlockParseResult result) noexcept;

    /**
     * TOML 文本 → 运行期 payload。
     *
     * `out_spec` 非空时回填**授权形态**（工具/回写用）；`out_data.block` 总是
     * 已摊平的物理行。
     */
    MaterialDataBlockParseResult ParseMaterialDataBlockFile(
        const char *content,
        int content_size,
        MaterialDataBlockFileData &out_data,
        MaterialDataBlockSpec *out_spec = nullptr) noexcept;

    /**
     * 授权形态 → 物理行。失败只有一个原因：`target` 不是已实现的内容行类型
     * （`GetGlobalSSBOTypeStructStride == 0`）。
     */
    bool BuildMaterialDataBlock(const MaterialDataBlockSpec &spec,
                                MaterialDataBlock &out_block) noexcept;

    /**
     * 授权形态 → TOML 文本（**按 `target` 投影**）。
     *
     * `MaterialDataRowSpec` 是三种内容行的**并集**，通用 `SaveToTomlText` 会把
     * 并集字段一个不落地写出来（PBR 块也会带上 `trans_color`），这样的文本再
     * 读回来会被跨类型键校验拒绝。本入口负责把「不属于本 target 的键」滤掉，
     * 是「程序批量生成数据块并落盘」的唯一写出口 —— 与
     * `ParseMaterialDataBlockFile` 的读入口严格互逆。
     */
    bool WriteMaterialDataBlockToml(const MaterialDataBlockSpec &spec,
                                    AnsiString &out_text) noexcept;

    /// 运行期块 → `MDB1` 字节流（含 16 字节头）。
    bool WriteMaterialDataBlockBinary(const MaterialDataBlock &block,
                                      ValueArray<uint8> &out_bytes) noexcept;

    /**
     * `MDB1` 字节流 → 视图（**零解析、零拷贝**）。
     *
     * 只做头校验：magic/version/row_type 合法、row_bytes 与该类型的物理行宽一致、
     * 行区长度不小于 `row_bytes * row_count`。失败时 `out_error` 给出原因。
     */
    bool ViewMaterialDataBlockBinary(const uint8 *data,
                                     uint64 size,
                                     MaterialDataBlockView &out_view,
                                     AnsiString &out_error) noexcept;

    /// `MDB1` 字节流 → 拥有副本的块（需要保值/改写时用）。
    bool ReadMaterialDataBlockBinary(const uint8 *data,
                                     uint64 size,
                                     MaterialDataBlock &out_block,
                                     AnsiString &out_error) noexcept;
}//namespace hgl::graph::mtl
