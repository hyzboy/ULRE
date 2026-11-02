#pragma once

/**
 * 材质数据块的 **FlatBuffers 形态**（工具侧）。
 *
 * 这里只做一件事：把已经有的两条链拼起来 ——
 *
 * ```
 * MaterialDataBlockSpec ⇄ hgl::serialize 值树 ⇄ FlatBuffers 缓冲
 * MaterialDataBlockSpec →  物理行 payload（MaterialDataBlockFile）
 * ```
 *
 * 于是「批量生成 → 落盘 → 工具检查 → 运行时取用」两侧共用同一个规约：
 * 生成器写 `MaterialDataBlockSpec`，编辑器读回来改两行，运行时摊平成 GPU 行。
 *
 * ── 为什么不写 `.fbs` ──────────────────────────────────────────────────────
 *
 * 反射元数据（`Class_MaterialDataBlockSpec`）已经描述了字段名、类别、元素
 * 类别与结构大小，`hgl::serialize::fb` 据此生成/读取表，不需要 flatc 产物，
 * 也不会有「schema 漂移但没人发现」这一类问题。真正需要**零解析直通 GPU** 的
 * 场景走 `MDB1`（`MaterialDataBlockFile.h`）—— 那是定长头 + 行区，比
 * FlatBuffers 的 vtable 更贴近显存布局。
 *
 * ── 与 TOML 入口的语义差异（有意为之） ────────────────────────────────────
 *
 * TOML 是**人工书写**的，所以 `ParseMaterialDataBlockFile` 会把「本 target 用不到
 * 的键」当错误报出来。FlatBuffers 是**程序产出**的，`MaterialDataRowSpec` 的并集
 * 字段（`color` / `trans_color` 等）一律会出现在缓冲里，因此这里不做跨类型键
 * 校验 —— 摊平时只读 `target` 认的那几个字段，其余字段不问。
 */

#include <hgl/asset/generated/MaterialDataBlockSerialize.gen.h>
#include <hgl/graph/asset/MaterialDataBlockSpec.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <hgl/mtl/MaterialDataBlock.h>
#include <hgl/mtl/MaterialDataBlockFile.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    /**
     * @brief 授权规约 → FlatBuffers 缓冲（完整缓冲，含根偏移）。
     *
     * `target` 必须是内容行类型：一个装不进任何内容行的块，落盘只会把问题
     * 推到运行时，因此在写出口就拒绝。
     */
    inline bool EncodeMaterialDataBlockFlat(const MaterialDataBlockSpec &spec,
                                           ValueArray<uint8> &out_bytes,
                                           AnsiString *out_error = nullptr)
    {
        out_bytes.Clear();

        if (GetGlobalSSBOTypeStructStride(spec.target) == 0)
        {
            if (out_error)
                *out_error = "target 不是内容行类型（无可摊平的行布局）";
            return false;
        }

        return hgl::serialize::fb::EncodeToFlatBuffers<MaterialDataBlockSpec>(
            out_bytes, spec, nullptr, out_error);
    }

    /**
     * @brief FlatBuffers 缓冲 → 授权规约（**先校验再解码**）。
     *
     * 外部来源（文件、网络、共享内存）一律走这条：校验失败时不碰 `out_spec`。
     */
    inline bool DecodeMaterialDataBlockFlat(const uint8 *data,
                                           uint64 size,
                                           MaterialDataBlockSpec &out_spec,
                                           AnsiString *out_error = nullptr)
    {
        out_spec = MaterialDataBlockSpec{};

        if (!data || size < hgl::serialize::kFlatBufferMinSize)
        {
            if (out_error)
                *out_error = "缓冲长度不足一个 FlatBuffers 根表";
            return false;
        }

        if (!hgl::serialize::fb::VerifyAndDecodeFlatBuffers<MaterialDataBlockSpec>(
                data, static_cast<size_t>(size), out_spec, nullptr, out_error))
            return false;

        if (GetGlobalSSBOTypeStructStride(out_spec.target) == 0)
        {
            if (out_error)
                *out_error = "target 不是内容行类型（无可摊平的行布局）";
            return false;
        }

        return true;
    }

    /// FlatBuffers 缓冲 → 物理行（解码 + 摊平一步到位）。
    inline bool BuildMaterialDataBlockFromFlat(const uint8 *data,
                                              uint64 size,
                                              MaterialDataBlock &out_block,
                                              AnsiString *out_error = nullptr)
    {
        out_block = MaterialDataBlock{};

        MaterialDataBlockSpec spec;
        if (!DecodeMaterialDataBlockFlat(data, size, spec, out_error))
            return false;

        if (!BuildMaterialDataBlock(spec, out_block))
        {
            if (out_error)
                *out_error = "摊平为物理行失败";
            return false;
        }

        return true;
    }
}//namespace hgl::graph::mtl
