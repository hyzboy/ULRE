#pragma once

/**
 * 统一序列化的**文件门面** —— `Archive` 家族的最外层。
 *
 * ── 它解决的问题 ──────────────────────────────────────────────────────────
 * 资产文件有两种物理形态（作者侧文本 `.env.toml` / 发行版二进制 `.env`），
 * 但**语义只有一份**。此前每个资产类各写一套 `SaveToToml` / `SaveToFlat` /
 * `LoadFromToml` / `LoadFromFlat`，等于把同一次字段遍历抄了四遍，且每加一种
 * 后端就要给每个资产类补一份。本文件把「选哪个后端」从资产类里彻底拿掉：
 *
 * ```
 * AssetObject::Serialize(Archive&)   ← 唯一一份字段遍历（AssetGen 生成）
 *            ▲
 *            │ 写方向：Archive::ForWrite(root)   读方向：Archive::ForRead(root)
 *            ▼
 *       Value 有序值树
 *            ▲
 *            │ 本文件按 ArchiveFormat 分派
 *            ▼
 *   TomlCodec（文本）        FlatCodec（FlatBuffers 二进制）
 * ```
 *
 * 于是「加一个 JSON 后端」= 加一个 Codec + 在 `ResolveArchiveFormatByExtension()`
 * 里多一条分支，**资产类一行都不用改**。
 *
 * ── 后缀约定 ──────────────────────────────────────────────────────────────
 * | 后缀 | 格式 | 谁读它 |
 * |------|------|--------|
 * | `*.toml` | `ArchiveFormat::Toml` | 作者手改、代码评审、diff |
 * | 其余带后缀的名字（`*.env` / `*.env.fb` / `*.scene.fb` / `*.bin`） | `ArchiveFormat::FlatBuffers` | 运行时装载 |
 * | 无后缀 | 无法判定 ⇒ `Auto` 解析失败 | — |
 *
 * 没有后缀一律报错而不是「猜一个」：写错位置时宁可当场失败，也不要悄悄产出
 * 一个没人能读的文件。
 *
 * ── 失败诊断 ──────────────────────────────────────────────────────────────
 * 本文件**不打日志**（纯函数，方便离线工具与单元测试复用）。全部失败原因经
 * 可选的 `AnsiString *out_error` 带回，格式统一为
 * `"<阶段>: path=<表路径> field=<键> detail=<原因>"`。
 */

#include <hgl/asset/AssetObject.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/serialize/Archive.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/serialize/Value.h>
#include <hgl/type/MemoryUtil.h>
#include <hgl/type/Smart.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>
#include <hgl/utf.h>

namespace hgl::serialize
{
    /**
     * @brief 资产文件的物理格式。
     *
     * `Auto` 只在入口参数上出现：由文件名后缀解析出真正的格式后再往下传，
     * 因此下游永远看不到 `Auto`。
     */
    enum class ArchiveFormat : uint8_t
    {
        Auto = 0,       ///<按文件名后缀解析（入口专用）
        Toml,           ///<作者侧文本；值树 → TOML 语法
        FlatBuffers,    ///<发行版二进制；值树 → 反射布局的 FlatBuffers 缓冲
    };

    inline const char *GetArchiveFormatName(const ArchiveFormat format)
    {
        switch (format)
        {
            case ArchiveFormat::Toml:        return "toml";
            case ArchiveFormat::FlatBuffers: return "flatbuffers";
            default:                         return "auto";
        }
    }

    /**
     * @brief 按文件名后缀解析物理格式。
     *
     * **只看最后一段带后缀的名字**：路径里的目录名带点（`./a.b/file`）不算。
     *
     * @param filename   文件名或完整路径
     * @param out_format 解析结果（成功时才写）
     * @return 能判定时返回 true；无后缀返回 false
     */
    inline bool ResolveArchiveFormatByExtension(const OSString &filename,
                                                ArchiveFormat &out_format)
    {
        if (filename.IsEmpty())
            return false;

        if (filename.EndsWithIgnoreCase(OS_TEXT(".toml")))
        {
            out_format = ArchiveFormat::Toml;
            return true;
        }

        // 其余格式一律二进制：`.env` / `.env.fb` / `.scene.fb` / `.bin` 都是
        // 「只有装载器才认」的形态。反过来猜成文本更危险 —— 文本解析器会把
        // 二进制读成乱码，然后报一个与真实问题无关的字段错。
        const os_char *name = filename.c_str();
        const int length = filename.Length();

        bool has_extension = false;

        for (int i = 0; i < length; ++i)
        {
            const os_char ch = name[i];

            if (ch == OS_TEXT('/') || ch == OS_TEXT('\\'))
                has_extension = false;              //目录名里的点作废
            else if (ch == OS_TEXT('.'))
                has_extension = true;
        }

        if (!has_extension)
            return false;

        out_format = ArchiveFormat::FlatBuffers;
        return true;
    }

    /// 把 `Archive` 记录的失败详情拼成一句可直接展示的原因
    inline AnsiString DescribeArchiveFailure(const Archive &ar)
    {
        AnsiString reason;

        if (ar.FailedPath()[0])
            reason += AnsiString(" path=") + ar.FailedPath();
        else if (ar.FailedField()[0])
            reason += AnsiString(" field=") + ar.FailedField();

        if (ar.ErrorMessage()[0])
            reason += AnsiString(" detail=") + ar.ErrorMessage();

        return reason;
    }

    namespace detail
    {
        /// 单个资产文件的读取上限：超过它基本可以判定「这不是资产文件」
        constexpr int64 ArchFileMaxSize = 64LL * 1024 * 1024;

        inline AnsiString PathToAnsi(const OSString &path)
        {
            const U8String text = ToU8String(path);
            return AnsiString(reinterpret_cast<const char *>(text.c_str()), text.Length());
        }

        /**
         * @brief 读整个文件到字节缓冲。
         *
         * 缓冲多要 1 字节并置 `'\0'`：TOML 解析器可以直接吃这个指针。
         */
        inline bool ReadWholeFile(const OSString &filename,
                                  AutoDeleteArray<uint8> &out_buffer,
                                  int64 &out_size,
                                  AnsiString *out_error)
        {
            out_size = 0;

            hgl::io::OpenFileInputStream opener(filename);
            if (!opener)
            {
                if (out_error)
                    *out_error = AnsiString("打开文件失败: ") + PathToAnsi(filename);
                return false;
            }

            const int64 size = opener->GetSize();

            if (size <= 0 || size > ArchFileMaxSize)
            {
                if (out_error)
                    *out_error = AnsiString("文件大小无效: ") + PathToAnsi(filename);

                return false;
            }

            if (!out_buffer.alloc(static_cast<size_t>(size) + 1))
            {
                if (out_error)
                    *out_error = AnsiString("读取缓冲分配失败: ") + PathToAnsi(filename);
                return false;
            }

            if (opener->Read(out_buffer.data(), size) != size)
            {
                if (out_error)
                    *out_error = AnsiString("文件读取不完整: ") + PathToAnsi(filename);
                return false;
            }

            out_buffer[static_cast<size_t>(size)] = 0;
            out_size = size;
            return true;
        }

        /// 整块字节写入文件（覆盖创建）
        inline bool WriteWholeFile(const OSString &filename,
                                   const void *data,
                                   const int64 size,
                                   AnsiString *out_error)
        {
            if (!data || size <= 0)
            {
                if (out_error)
                    *out_error = AnsiString("写入内容为空: ") + PathToAnsi(filename);
                return false;
            }

            hgl::io::FileOutputStream *fos = hgl::io::CreateFileOutputStream(filename);
            if (!fos)
            {
                if (out_error)
                    *out_error = AnsiString("创建文件失败: ") + PathToAnsi(filename);
                return false;
            }

            const int64 written = fos->Write(data, size);
            delete fos;

            if (written != size)
            {
                if (out_error)
                    *out_error = AnsiString("文件写入不完整: ") + PathToAnsi(filename);
                return false;
            }

            return true;
        }

        /// 值树 → 文件（`format` 必须是已解析的确定格式，不接受 `Auto`）
        inline bool SaveValueTreeToFile(const Value &root,
                                        const OSString &filename,
                                        const ArchiveFormat format,
                                        const hgl::asset::ClassMetaInfo *meta,
                                        AnsiString *out_error);
    }//namespace detail

    // ── 值树 ⇄ 字节（FlatBuffers）────────────────────────────────────────────
    //
    // 二进制布局无法从值树本身推断（voffset、内联 struct、向量元素类型），必须
    // 由 `ClassMetaInfo` 决定 —— 这正是「反射元数据是唯一真源」的落点。

    /// 值树 + 反射元数据 → 完整 FlatBuffers 缓冲
    inline bool SaveValueToBytes(const Value &root,
                                 const hgl::asset::ClassMetaInfo &meta,
                                 ValueArray<uint8> &out_bytes,
                                 AnsiString *out_error = nullptr)
    {
        out_bytes.Clear();

        flatbuffers::FlatBufferBuilder fbb;
        fb::EncodeToBuffer(fbb, root, meta, nullptr);

        const int size = static_cast<int>(fbb.GetSize());

        if (size <= 0 || !out_bytes.Resize(size))
        {
            if (out_error)
                *out_error = "FlatBuffers 输出缓冲分配失败";
            return false;
        }

        hgl::mem_copy(out_bytes.GetData(),
                      reinterpret_cast<const uint8 *>(fbb.GetBufferPointer()),
                      static_cast<size_t>(size));

        return true;
    }

    /// 完整 FlatBuffers 缓冲 → 值树（先校验，再解码）
    inline bool LoadValueFromBytes(const uint8 *data,
                                   const size_t size,
                                   const hgl::asset::ClassMetaInfo &meta,
                                   Value &out_root,
                                   AnsiString *out_error = nullptr)
    {
        out_root.Reset();

        if (!data || size < FLATBUFFERS_MIN_BUFFER_SIZE)
        {
            if (out_error)
                *out_error = "FlatBuffers 缓冲长度不足一个根表";
            return false;
        }

        if (!fb::VerifyBuffer(data, size, meta, nullptr))
        {
            if (out_error)
                *out_error = "FlatBuffers 校验失败：缓冲结构不合法";
            return false;
        }

        if (!fb::DecodeFromBuffer(data, size, meta, out_root))
        {
            if (out_error)
                *out_error = "FlatBuffers 根表解码失败";
            return false;
        }

        return true;
    }

    // ── AssetObject 门面 ─────────────────────────────────────────────────────
    //
    // 派生类只需要实现 `Serialize(Archive&)` —— 唯一的一份字段遍历。下面这些
    // 函数负责「值树 ⇄ 文本 / 字节 / 文件」，**不认识任何具体资产类型**。

    /**
     * @brief `AssetObject` → 值树（调用派生类的 `Serialize()`，写方向）。
     */
    inline bool WriteAssetObjectValue(hgl::asset::AssetObject &obj,
                                      Value &out_root,
                                      AnsiString *out_error = nullptr)
    {
        Archive ar = Archive::ForWrite(out_root);

        if (!obj.Serialize(ar) || !ar.Ok())
        {
            if (out_error)
                *out_error = ar.Ok()
                           ? AnsiString("该资产类型未实现统一序列化（Serialize 返回 false）")
                           : DescribeArchiveFailure(ar);
            return false;
        }

        return true;
    }

    /**
     * @brief 值树 → `AssetObject`（调用派生类的 `Serialize()`，读方向）。
     *
     * 缺键按派生类的类内初始值处理 —— 这是 `Archive` 层的既有语义，本层不干预。
     */
    inline bool ReadAssetObjectValue(hgl::asset::AssetObject &obj,
                                     const Value &root,
                                     AnsiString *out_error = nullptr)
    {
        Archive ar = Archive::ForRead(root);

        if (!obj.Serialize(ar) || !ar.Ok())
        {
            if (out_error)
                *out_error = ar.Ok()
                           ? AnsiString("该资产类型未实现统一序列化（Serialize 返回 false）")
                           : DescribeArchiveFailure(ar);
            return false;
        }

        return true;
    }

    /// `AssetObject` → TOML 文本
    inline bool SaveAssetObjectToText(hgl::asset::AssetObject &obj,
                                      AnsiString &out_text,
                                      AnsiString *out_error = nullptr)
    {
        out_text.Clear();

        Value root;

        if (!WriteAssetObjectValue(obj, root, out_error))
            return false;

        out_text = ToTomlText(root);

        if (out_text.IsEmpty())
        {
            if (out_error)
                *out_error = "TOML 文本格式化为空";
            return false;
        }

        return true;
    }

    /// TOML 文本 → `AssetObject`
    inline bool LoadAssetObjectFromText(hgl::asset::AssetObject &obj,
                                        const char *content,
                                        const size_t size,
                                        AnsiString *out_error = nullptr)
    {
        if (!content || size == 0)
        {
            if (out_error)
                *out_error = "TOML 内容为空";
            return false;
        }

        Value root;
        TomlParseError toml_error;

        if (!FromTomlText(content, size, root, &toml_error))
        {
            if (out_error)
                *out_error = AnsiString("toml: ") + toml_error.Format();
            return false;
        }

        return ReadAssetObjectValue(obj, root, out_error);
    }

    /// `AssetObject` → FlatBuffers 缓冲
    inline bool SaveAssetObjectToBytes(hgl::asset::AssetObject &obj,
                                       ValueArray<uint8> &out_bytes,
                                       AnsiString *out_error = nullptr)
    {
        out_bytes.Clear();

        const hgl::asset::ClassMetaInfo *meta = obj.GetClassMetaInfo();

        if (!meta)
        {
            if (out_error)
                *out_error = "该资产类型没有反射元数据（ClassMetaInfo），无法编码为 FlatBuffers";
            return false;
        }

        Value root;

        if (!WriteAssetObjectValue(obj, root, out_error))
            return false;

        return SaveValueToBytes(root, *meta, out_bytes, out_error);
    }

    /// FlatBuffers 缓冲 → `AssetObject`
    inline bool LoadAssetObjectFromBytes(hgl::asset::AssetObject &obj,
                                         const uint8 *data,
                                         const size_t size,
                                         AnsiString *out_error = nullptr)
    {
        const hgl::asset::ClassMetaInfo *meta = obj.GetClassMetaInfo();

        if (!meta)
        {
            if (out_error)
                *out_error = "该资产类型没有反射元数据（ClassMetaInfo），无法解码 FlatBuffers";
            return false;
        }

        Value root;

        if (!LoadValueFromBytes(data, size, *meta, root, out_error))
            return false;

        return ReadAssetObjectValue(obj, root, out_error);
    }

    namespace detail
    {
        inline bool SaveValueTreeToFile(const Value &root,
                                        const OSString &filename,
                                        const ArchiveFormat format,
                                        const hgl::asset::ClassMetaInfo *meta,
                                        AnsiString *out_error)
        {
            if (format == ArchiveFormat::Toml)
            {
                const AnsiString text = ToTomlText(root);

                if (text.IsEmpty())
                {
                    if (out_error)
                        *out_error = "TOML 文本格式化为空";
                    return false;
                }

                return WriteWholeFile(filename, text.c_str(), text.Length(), out_error);
            }

            if (!meta)
            {
                if (out_error)
                    *out_error = "FlatBuffers 需要反射元数据（ClassMetaInfo）";
                return false;
            }

            ValueArray<uint8> bytes;

            if (!SaveValueToBytes(root, *meta, bytes, out_error))
                return false;

            return WriteWholeFile(filename, bytes.GetData(), bytes.GetCount(), out_error);
        }

        /// 文件 → 值树（`format` 由入口解析；`meta` 只有二进制分支需要）
        inline bool LoadValueTreeFromFile(const OSString &filename,
                                         const ArchiveFormat format,
                                         const hgl::asset::ClassMetaInfo *meta,
                                         Value &out_root,
                                         AnsiString *out_error)
        {
            AutoDeleteArray<uint8> buffer;
            int64 size = 0;

            if (!ReadWholeFile(filename, buffer, size, out_error))
                return false;

            if (format == ArchiveFormat::Toml)
            {
                TomlParseError toml_error;

                if (!FromTomlText(reinterpret_cast<const char *>(buffer.data()),
                                  static_cast<size_t>(size), out_root, &toml_error))
                {
                    if (out_error)
                        *out_error = AnsiString("toml: ") + toml_error.Format();
                    return false;
                }

                return true;
            }

            if (!meta)
            {
                if (out_error)
                    *out_error = "FlatBuffers 需要反射元数据（ClassMetaInfo）";
                return false;
            }

            return LoadValueFromBytes(buffer.data(), static_cast<size_t>(size),
                                      *meta, out_root, out_error);
        }
    }//namespace detail

    /**
     * @brief `AssetObject` → 文件：格式由后缀（或显式 `format`）决定。
     *
     * @param obj       资产（读方向会写回，因此取非 const 引用）
     * @param filename  目标文件
     * @param format    显式格式；`Auto` 时按后缀解析
     * @param out_error 失败原因
     */
    inline bool SaveAssetObjectToFile(hgl::asset::AssetObject &obj,
                                      const OSString &filename,
                                      const ArchiveFormat format = ArchiveFormat::Auto,
                                      AnsiString *out_error = nullptr)
    {
        ArchiveFormat resolved = format;

        if (resolved == ArchiveFormat::Auto
         && !ResolveArchiveFormatByExtension(filename, resolved))
        {
            if (out_error)
                *out_error = AnsiString("无法从文件名判定格式（需 .toml 或其他带后缀的名字）: ")
                           + detail::PathToAnsi(filename);

            return false;
        }

        Value root;

        if (!WriteAssetObjectValue(obj, root, out_error))
            return false;

        return detail::SaveValueTreeToFile(root, filename, resolved,
                                           obj.GetClassMetaInfo(), out_error);
    }

    /**
     * @brief 文件 → `AssetObject`：格式由后缀（或显式 `format`）决定。
     */
    inline bool LoadAssetObjectFromFile(hgl::asset::AssetObject &obj,
                                        const OSString &filename,
                                        const ArchiveFormat format = ArchiveFormat::Auto,
                                        AnsiString *out_error = nullptr)
    {
        ArchiveFormat resolved = format;

        if (resolved == ArchiveFormat::Auto
         && !ResolveArchiveFormatByExtension(filename, resolved))
        {
            if (out_error)
                *out_error = AnsiString("无法从文件名判定格式（需 .toml 或其他带后缀的名字）: ")
                           + detail::PathToAnsi(filename);

            return false;
        }

        Value root;

        if (!detail::LoadValueTreeFromFile(filename, resolved,
                                           obj.GetClassMetaInfo(), root, out_error))
            return false;

        return ReadAssetObjectValue(obj, root, out_error);
    }

    // ── 值语义结构门面 ──────────────────────────────────────────────────────
    //
    // 不继承 `AssetObject` 的纯数据规约（绝大多数 `HGL_ASSET_STRUCT`）走这里：
    // 字段遍历来自生成的 `StructSerializer<T>::Do()`，布局来自 `Meta()`。

    /// `T` → FlatBuffers 缓冲
    template <typename T>
    inline bool SaveStructToBytes(const T &src,
                                  ValueArray<uint8> &out_bytes,
                                  AnsiString *out_error = nullptr)
    {
        out_bytes.Clear();

        Value root;
        Archive ar = Archive::ForWrite(root);
        StructSerializer<T>::Do(ar, src);

        if (!ar.Ok())
        {
            if (out_error)
                *out_error = DescribeArchiveFailure(ar);
            return false;
        }

        return SaveValueToBytes(root, *StructSerializer<T>::Meta(), out_bytes, out_error);
    }

    /// FlatBuffers 缓冲 → `T`（先校验，再解码）
    template <typename T>
    inline bool LoadStructFromBytes(const uint8 *data,
                                    const size_t size,
                                    T &out,
                                    AnsiString *out_error = nullptr)
    {
        Value root;

        if (!LoadValueFromBytes(data, size, *StructSerializer<T>::Meta(), root, out_error))
            return false;

        return ReadFromValue<T>(root, out, nullptr);
    }

    /// `T` → 文件（`format` = `Auto` 时按后缀解析）
    template <typename T>
    inline bool SaveStructToFile(const T &src,
                                 const OSString &filename,
                                 const ArchiveFormat format = ArchiveFormat::Auto,
                                 AnsiString *out_error = nullptr)
    {
        ArchiveFormat resolved = format;

        if (resolved == ArchiveFormat::Auto
         && !ResolveArchiveFormatByExtension(filename, resolved))
        {
            if (out_error)
                *out_error = AnsiString("无法从文件名判定格式（需 .toml 或其他带后缀的名字）: ")
                           + detail::PathToAnsi(filename);

            return false;
        }

        Value root;
        Archive ar = Archive::ForWrite(root);
        StructSerializer<T>::Do(ar, src);

        if (!ar.Ok())
        {
            if (out_error)
                *out_error = DescribeArchiveFailure(ar);
            return false;
        }

        return detail::SaveValueTreeToFile(root, filename, resolved,
                                           StructSerializer<T>::Meta(), out_error);
    }

    /// 文件 → `T`（缺键保留 `out` 的类内初始值）
    template <typename T>
    inline bool LoadStructFromFile(const OSString &filename,
                                   T &out,
                                   const ArchiveFormat format = ArchiveFormat::Auto,
                                   AnsiString *out_error = nullptr)
    {
        ArchiveFormat resolved = format;

        if (resolved == ArchiveFormat::Auto
         && !ResolveArchiveFormatByExtension(filename, resolved))
        {
            if (out_error)
                *out_error = AnsiString("无法从文件名判定格式（需 .toml 或其他带后缀的名字）: ")
                           + detail::PathToAnsi(filename);

            return false;
        }

        Value root;

        if (!detail::LoadValueTreeFromFile(filename, resolved,
                                           StructSerializer<T>::Meta(), root, out_error))
            return false;

        return ReadFromValue<T>(root, out, nullptr);
    }
}//namespace hgl::serialize
