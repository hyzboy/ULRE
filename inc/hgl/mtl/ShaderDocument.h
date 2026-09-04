#pragma once

#include <hgl/type/String.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    enum class ShaderDocumentBlockKind
    {
        Version,
        Extension,
        Define,
        Resource,
        Interface,
        Module,
        Function,
        MainBody,
        Raw
    };

    struct ShaderDocumentSource
    {
        AnsiString material;
        AnsiString stage;
        AnsiString module;
        AnsiString path;
        AnsiString logical_name;
    };

    struct ShaderDocumentDiagnostic
    {
        AnsiString code;
        AnsiString message;
        int block_index = -1;
        ShaderDocumentSource source;
    };

    using ShaderDocumentDiagnostics = hgl::ManagedArray<ShaderDocumentDiagnostic>;

    struct ShaderDocumentBlock
    {
        ShaderDocumentBlockKind kind = ShaderDocumentBlockKind::Raw;
        AnsiString text;
        ShaderDocumentSource source;
    };

    class ShaderDocument
    {
        hgl::ManagedArray<ShaderDocumentBlock> blocks;

    public:
        void Clear();
        int GetBlockCount() const;
        const ShaderDocumentBlock &GetBlock(int index) const;

        void Add(ShaderDocumentBlockKind kind,
                 const AnsiString &text,
                 const ShaderDocumentSource &source = {});

        /// 整文档序列化：校验恰有一个 Version 块且位于首位，然后拼接全部块
        bool Serialize(AnsiString &out_text,
                       ShaderDocumentDiagnostics &out_diagnostics) const;

        /// 片段序列化：无校验直接拼接——供没有 Version 块的子文档
        ///（输入/provider 片段）使用，合并前的中间形态
        bool SerializeFragment(AnsiString &out_text,
                               ShaderDocumentDiagnostics &out_diagnostics) const;

        hgl::uint64 GetSerializedHash(ShaderDocumentDiagnostics &out_diagnostics) const;
    };
}
