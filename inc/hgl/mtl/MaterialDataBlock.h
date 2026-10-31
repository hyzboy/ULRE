#pragma once

/**
 * 材质数据块的**运行期形态**：一段与 GPU 行逐字节同构的连续 payload。
 *
 * 上游是 `hgl::graph::MaterialDataBlockSpec`（文件里的授权形态，见
 * `inc/hgl/graph/asset/MaterialDataBlockSpec.h`）；本结构只关心「往哪个池、
 * 多少行、每行多少字节」，不含任何字段语义 —— 摊平之后 shader 侧的结构体布局
 * 才是唯一真源（`inc/hgl/graph/ssbo/MaterialDataRows.h`）。
 *
 * ── 两种来源，一条上船路径 ──────────────────────────────────────────────────
 *
 * · TOML 授权：`ParseMaterialDataBlockFile` → `BuildMaterialDataBlock` → 本结构
 *   （`payload` 是所有权数据，`GetData()` 直接可 `memcpy` 到 mapped 指针）；
 * · 二进制（`MDB1` 文件）：`ViewMaterialDataBlockBinary` → `MaterialDataBlockView`
 *   （**零解析、零拷贝**：只校验 16 字节头，然后指向文件缓冲里的行区）。
 *
 * 两者都不需要任何逐字段解析即可上传，区别只是「谁拥有这段字节」。
 */

#include <hgl/CoreType.h>
#include <hgl/graph/asset/MaterialDataBlockSpec.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    /// 二进制块文件头长度（`MDB1` 4 + version 2 + row_type 1 + reserved 1 + 4 + 4）。
    inline constexpr uint32 kMaterialDataBlockBinaryHeaderSize = 16;

    /// 二进制块格式版本。
    inline constexpr uint16 kMaterialDataBlockBinaryVersion = 1;

    /// 单个块的**行数上限**。存在的唯一理由是挡住失控文件（行数写错一位就是 GB 级
    /// 分配），不是池容量约束 —— 池容量由 `GlobalSSBOBufferRegistry` 决定。
    inline constexpr uint32 kMaterialDataBlockMaxRows = 1u << 16;

    /// 一个块（拥有 payload）。
    struct MaterialDataBlock
    {
        GlobalSSBOType     type      = GlobalSSBOType::PBRSurface;  ///<内容行类型（决定落哪个池）
        uint32             row_bytes = 0;                           ///<物理行宽（= GetGlobalSSBOTypeStructStride）
        ValueArray<uint8>  payload;                                 ///<row_bytes * row_count 字节

        uint32 GetRowCount() const noexcept
        {
            return (row_bytes > 0) ? static_cast<uint32>(payload.GetCount()) / row_bytes : 0;
        }

        uint32 GetByteSize() const noexcept
        {
            return static_cast<uint32>(payload.GetCount());
        }

        const void *GetData() const noexcept
        {
            return payload.GetData();
        }

        bool IsValid() const noexcept
        {
            return row_bytes > 0
                && payload.GetCount() > 0
                && (static_cast<uint32>(payload.GetCount()) % row_bytes) == 0;
        }
    };

    /// 一个块（**不拥有** payload，指向调用方缓冲）。零拷贝上传用。
    struct MaterialDataBlockView
    {
        GlobalSSBOType type      = GlobalSSBOType::PBRSurface;
        uint32         row_bytes = 0;
        uint32         row_count = 0;
        const uint8   *rows      = nullptr;

        uint32 GetByteSize() const noexcept
        {
            return row_bytes * row_count;
        }

        bool IsValid() const noexcept
        {
            return row_bytes > 0 && row_count > 0 && rows != nullptr;
        }
    };
}//namespace hgl::graph::mtl
