#include <hgl/mtl/MaterialDataBlockFile.h>

#include <hgl/asset/generated/MaterialDataBlockMeta.gen.h>
#include <hgl/asset/generated/MaterialDataBlockSerialize.gen.h>
#include <hgl/graph/ssbo/MaterialDataRows.h>
#include <hgl/serialize/Archive.h>
#include <hgl/serialize/MetaSchema.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/serialize/Value.h>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        /// 诊断上下文：一次解析只记**第一个**失败（与 `MaterialRecipeFile.cpp` 同构）。
        struct ParseErrorContext
        {
            AnsiString table = "root";
            AnsiString key;
            AnsiString value;
            AnsiString detail;
            int line = 0;
            bool recorded = false;

            void RecordIn(const AnsiString &table_path,
                          const char *in_key,
                          const char *in_value,
                          const char *in_detail)
            {
                if (recorded)
                    return;

                table = table_path.IsEmpty() ? AnsiString("root") : table_path;
                key = in_key ? in_key : "";
                value = in_value ? in_value : "";
                detail = in_detail ? in_detail : "";
                recorded = true;
            }

            void Record(const char *in_key, const char *in_value, const char *in_detail)
            {
                RecordIn(AnsiString("root"), in_key, in_value, in_detail);
            }
        };

        /// `render_state.pipeline` 风格的键路径 → 「表 + 键」（与配方解析同一口径）。
        void SplitTablePath(const AnsiString &path, AnsiString &table, AnsiString &key)
        {
            key = path;
            table = "root";

            if (path.IsEmpty())
                return;

            const char *begin = path.c_str();
            const char *last_dot = nullptr;
            for (const char *cursor = begin; cursor[0]; ++cursor)
            {
                if (cursor[0] == '.')
                    last_dot = cursor;
            }

            if (!last_dot)
                return;

            table = AnsiString(begin, static_cast<int>(last_dot - begin));
            key = AnsiString(last_dot + 1);
        }

        uint32 RowBytesFor(const GlobalSSBOType type) noexcept
        {
            return GetGlobalSSBOTypeStructStride(type);
        }

        /// 该类型是不是「材质内容行」（三种表面数据行之一）。
        bool IsContentRowType(const GlobalSSBOType type) noexcept
        {
            return RowBytesFor(type) > 0;
        }

        /// 本 target 认哪些行键。
        ///
        /// 这张表是 `MaterialDataRowSpec` 各字段的**逐 target 投影**：它不复制类型/
        /// 偏移（那是 AssetGen 的唯一真源），只回答「这个键对这类行有没有意义」。
        /// 给 `PBRSurface` 的块写 `trans_color` 是明确的错误，不能静默忽略 ——
        /// 静默忽略会让「我明明写了却没有任何效果」变成最难查的一类问题。
        bool KeyAllowedForTarget(const GlobalSSBOType type, const char *key) noexcept
        {
            if (!key || !key[0])
                return false;

            switch (type)
            {
            case GlobalSSBOType::PBRSurface:
                return strcmp(key, "base_color") == 0
                    || strcmp(key, "metallic") == 0
                    || strcmp(key, "roughness") == 0
                    || strcmp(key, "normal_scale") == 0
                    || strcmp(key, "fresnel") == 0;

            case GlobalSSBOType::EmissiveSurface:
                return strcmp(key, "color") == 0;

            case GlobalSSBOType::TransmissionSurface:
                return strcmp(key, "trans_color") == 0;

            default:
                return false;
            }
        }

        /// 授权行 → 物理行（逐字节等于 `MaterialDataRows.h` 的结构体）。
        bool PackRow(const MaterialDataRowSpec &src,
                     const GlobalSSBOType type,
                     uint8 *dst) noexcept
        {
            switch (type)
            {
            case GlobalSSBOType::PBRSurface:
            {
                ssbo::PBRSurfaceRow row;
                row.base_color   = src.base_color;
                row.metallic     = src.metallic;
                row.roughness    = src.roughness;
                row.normal_scale = src.normal_scale;
                row.fresnel      = src.fresnel;
                memcpy(dst, &row, sizeof(row));
                return true;
            }
            case GlobalSSBOType::EmissiveSurface:
            {
                ssbo::EmissiveSurfaceRow row;
                row.color = src.color;
                memcpy(dst, &row, sizeof(row));
                return true;
            }
            case GlobalSSBOType::TransmissionSurface:
            {
                ssbo::TransmissionSurfaceRow row{};
                row.trans_color = src.trans_color.packUnorm4x8();
                memcpy(dst, &row, sizeof(row));
                return true;
            }
            default:
                return false;
            }
        }

        /// 严格键校验：schema 层（`Class_MaterialDataBlockSpec`）之后逐行再过一遍
        /// 「本 target 认不认这个键」。
        bool ValidateRowKeysForTarget(const hgl::serialize::Value &root,
                                      const GlobalSSBOType type,
                                      ParseErrorContext &error)
        {
            const hgl::serialize::Value *rows = root.FindFieldExact("rows");
            if (!rows || !rows->IsArray())
                return true;    // 形态问题交给生成的序列化去报，措辞更准

            const int count = rows->GetElementCount();
            for (int i = 0; i < count; ++i)
            {
                const hgl::serialize::Value *row = rows->GetElement(i);
                if (!row || !row->IsTable())
                    continue;

                AnsiString path = "rows[";
                path += AnsiString::numberOf(i);
                path += "]";

                const int field_count = row->GetFieldCount();
                for (int f = 0; f < field_count; ++f)
                {
                    const char *key = row->GetFieldName(f);

                    if (KeyAllowedForTarget(type, key))
                        continue;

                    error.RecordIn(path, key, nullptr,
                                   "该键不属于 target 对应的行类型");
                    return false;
                }
            }

            return true;
        }

        void ApplyCodecFailure(const hgl::serialize::Archive &archive,
                               ParseErrorContext &error)
        {
            AnsiString table;
            AnsiString key;
            SplitTablePath(AnsiString(archive.FailedPath()), table, key);

            error.RecordIn(table, key.c_str(), nullptr, archive.ErrorMessage());
        }

        bool ReadSpec(const hgl::serialize::Value &root,
                      MaterialDataBlockSpec &spec,
                      ParseErrorContext &error)
        {
            hgl::serialize::Archive archive = hgl::serialize::Archive::ForRead(root);
            hgl::serialize::StructSerializer<MaterialDataBlockSpec>::Do(archive, spec);
            if (archive.Ok())
                return true;

            // 生成 codec 的措辞是通用的「字段类型或取值不合法」；把它的定位
            // 拆成「表 + 键」，与配方解析的诊断格式保持一致。
            ApplyCodecFailure(archive, error);
            return false;
        }

        void WriteU16(uint8 *dst, const uint16 value) noexcept
        {
            dst[0] = static_cast<uint8>(value & 0xFF);
            dst[1] = static_cast<uint8>((value >> 8) & 0xFF);
        }

        void WriteU32(uint8 *dst, const uint32 value) noexcept
        {
            dst[0] = static_cast<uint8>(value & 0xFF);
            dst[1] = static_cast<uint8>((value >> 8) & 0xFF);
            dst[2] = static_cast<uint8>((value >> 16) & 0xFF);
            dst[3] = static_cast<uint8>((value >> 24) & 0xFF);
        }

        uint16 ReadU16(const uint8 *src) noexcept
        {
            return static_cast<uint16>(src[0] | (static_cast<uint16>(src[1]) << 8));
        }

        uint32 ReadU32(const uint8 *src) noexcept
        {
            return static_cast<uint32>(src[0])
                 | (static_cast<uint32>(src[1]) << 8)
                 | (static_cast<uint32>(src[2]) << 16)
                 | (static_cast<uint32>(src[3]) << 24);
        }
    }//namespace

    const char *GetMaterialDataBlockParseResultName(
        MaterialDataBlockParseResult result) noexcept
    {
        switch (result)
        {
        case MaterialDataBlockParseResult::Skipped:      return "Skipped";
        case MaterialDataBlockParseResult::OK:           return "OK";
        case MaterialDataBlockParseResult::InvalidValue: return "InvalidValue";
        case MaterialDataBlockParseResult::UnknownKey:   return "UnknownKey";
        }

        return "Unknown";
    }

    bool BuildMaterialDataBlock(const MaterialDataBlockSpec &spec,
                                MaterialDataBlock &out_block) noexcept
    {
        out_block = MaterialDataBlock{};

        if (!IsContentRowType(spec.target))
            return false;

        const int row_count = spec.rows.GetCount();
        if (row_count <= 0 || static_cast<uint32>(row_count) > kMaterialDataBlockMaxRows)
            return false;

        const uint32 row_bytes = RowBytesFor(spec.target);
        const uint32 byte_size = row_bytes * static_cast<uint32>(row_count);

        ValueArray<uint8> payload;
        if (!payload.Resize(static_cast<int>(byte_size)))
            return false;

        uint8 *dst = payload.GetData();
        const MaterialDataRowSpec *src = spec.rows.GetData();

        // 逐行摊平：每行写进 payload 的字节与 GPU 侧结构体逐字节相同，
        // 因此上传时不需要任何再次解释（`AcquireRowRange` + `memcpy` 即可）。
        for (int i = 0; i < row_count; ++i, dst += row_bytes)
        {
            if (!PackRow(src[i], spec.target, dst))
                return false;
        }

        out_block.type      = spec.target;
        out_block.row_bytes = row_bytes;
        out_block.payload   = std::move(payload);
        return true;
    }

    MaterialDataBlockParseResult ParseMaterialDataBlockFile(
        const char *content,
        const int content_size,
        MaterialDataBlockFileData &out_data,
        MaterialDataBlockSpec *out_spec) noexcept
    {
        out_data = MaterialDataBlockFileData{};
        if (out_spec)
            *out_spec = MaterialDataBlockSpec{};
        if (!content || content_size <= 0)
            return MaterialDataBlockParseResult::Skipped;

        ParseErrorContext error;
        MaterialDataBlockSpec spec;
        MaterialDataBlockParseResult result = MaterialDataBlockParseResult::OK;

        // 文本 → 值树只走 `hgl::serialize` 的 TOML 后端；结构解析全部由生成的
        // `StructSerializer<MaterialDataBlockSpec>` 驱动。
        hgl::serialize::Value root;
        hgl::serialize::TomlParseError toml_error;
        const bool parsed =
            hgl::serialize::FromTomlText(content, static_cast<size_t>(content_size),
                                         root, &toml_error);

        const hgl::serialize::Value *schema_node =
            parsed ? root.FindFieldExact("schema") : nullptr;
        int64_t schema_value = 0;

        if (!parsed)
        {
            error.line = toml_error.line;
            error.detail = toml_error.detail.IsEmpty() ? toml_error.title
                                                       : toml_error.detail;
            result = MaterialDataBlockParseResult::InvalidValue;
        }
        else if (!root.IsTable())
        {
            error.Record(nullptr, nullptr, "根节点必须是表");
            result = MaterialDataBlockParseResult::InvalidValue;
        }
        else if (!schema_node || !schema_node->GetInt(schema_value) || schema_value != 1)
        {
            error.Record("schema", nullptr, "必须存在且等于 1");
            result = MaterialDataBlockParseResult::InvalidValue;
        }
        else if (!root.FindFieldExact("name"))
        {
            error.Record("name", nullptr, "缺少必需字符串");
            result = MaterialDataBlockParseResult::InvalidValue;
        }
        else if (!root.FindFieldExact("target"))
        {
            // `target` 有类内缺省（PBRSurface），但**块必须显式声明**它落哪个池：
            // 一个「什么都没写就默认 PBR」的数据块，错起来无声无息。
            error.Record("target", nullptr, "缺少必需字段（内容行类型名）");
            result = MaterialDataBlockParseResult::InvalidValue;
        }
        else
        {
            // 严格键校验的唯一真源是生成的 `ClassMetaInfo`；这里只判「键名是否合法」，
            // 取值/形态错误交给生成的序列化。
            AnsiString bad_path;
            AnsiString bad_key;
            if (!hgl::serialize::ValidateKeysByMeta(
                    root, hgl::asset::generated::Class_MaterialDataBlockSpec,
                    AnsiString(), bad_path, bad_key))
            {
                error.RecordIn(bad_path, bad_key.c_str(), nullptr, "未知键");
                result = MaterialDataBlockParseResult::UnknownKey;
            }
            else if (!ReadSpec(root, spec, error))
            {
                result = MaterialDataBlockParseResult::InvalidValue;
            }
            else if (!IsContentRowType(spec.target))
            {
                error.Record("target", hgl::serialize::EnumNameOf(spec.target),
                             "该类型不是材质内容行（无可摊平的行布局）");
                result = MaterialDataBlockParseResult::InvalidValue;
            }
            else if (spec.name.IsEmpty())
            {
                error.Record("name", nullptr, "必须是非空字符串");
                result = MaterialDataBlockParseResult::InvalidValue;
            }
            else if (spec.rows.GetCount() <= 0)
            {
                error.Record("rows", nullptr, "至少要有一行");
                result = MaterialDataBlockParseResult::InvalidValue;
            }
            else if (static_cast<uint32>(spec.rows.GetCount()) > kMaterialDataBlockMaxRows)
            {
                error.Record("rows", nullptr, "行数超过上限（见 kMaterialDataBlockMaxRows）");
                result = MaterialDataBlockParseResult::InvalidValue;
            }
            else if (!ValidateRowKeysForTarget(root, spec.target, error))
            {
                result = MaterialDataBlockParseResult::UnknownKey;
            }
            else if (!BuildMaterialDataBlock(spec, out_data.block))
            {
                error.Record("rows", nullptr, "摊平为物理行失败");
                result = MaterialDataBlockParseResult::InvalidValue;
            }
        }

        if (result != MaterialDataBlockParseResult::OK)
        {
            out_data.parse_error.table  = error.table;
            out_data.parse_error.key    = error.key;
            out_data.parse_error.value  = error.value;
            out_data.parse_error.detail = error.detail;
            out_data.parse_error.line   = error.line;
        }
        else if (out_spec)
            *out_spec = std::move(spec);

        return result;
    }

    bool WriteMaterialDataBlockToml(const MaterialDataBlockSpec &spec,
                                    AnsiString &out_text) noexcept
    {
        out_text.Clear();

        if (!IsContentRowType(spec.target))
            return false;

        // 先走通用序列化拿到「并集形态」的值树：键名拼写、标量/向量格式、
        // 枚举文本全部由 AssetGen 产物单点决定，这里不重复实现。
        hgl::serialize::Value full;
        hgl::serialize::Archive archive = hgl::serialize::Archive::ForWrite(full);
        archive.WriteTo(full, spec);
        if (!archive.Ok())
            return false;

        const hgl::serialize::Value *full_rows = full.FindFieldExact("rows");
        if (!full_rows || !full_rows->IsArray())
            return false;

        // 再按 target 投影：只保留本行类型认得的键。这一步是读入口
        // （`ValidateRowKeysForTarget`）的严格逆运算。
        hgl::serialize::Value root;
        root.SetTable();

        for (int i = 0; i < full.GetFieldCount(); ++i)
        {
            const char *name = full.GetFieldName(i);
            if (!name || strcmp(name, "rows") == 0)
                continue;

            hgl::serialize::Value *dst = root.AddField(name, 0);
            const hgl::serialize::Value *src = full.GetFieldValue(i);
            if (!dst || !src)
                return false;

            dst->CopyFrom(*src);
        }

        hgl::serialize::Value *rows_out = root.AddField("rows", 0);
        if (!rows_out)
            return false;

        rows_out->SetArray();

        const int row_count = full_rows->GetElementCount();
        for (int i = 0; i < row_count; ++i)
        {
            const hgl::serialize::Value *row = full_rows->GetElement(i);
            if (!row || !row->IsTable())
                return false;

            hgl::serialize::Value *elem = rows_out->AddElement();
            if (!elem)
                return false;

            elem->SetTable();

            for (int f = 0; f < row->GetFieldCount(); ++f)
            {
                const char *key = row->GetFieldName(f);
                if (!KeyAllowedForTarget(spec.target, key))
                    continue;

                hgl::serialize::Value *dst = elem->AddField(key, 0);
                const hgl::serialize::Value *src = row->GetFieldValue(f);
                if (!dst || !src)
                    return false;

                dst->CopyFrom(*src);
            }
        }

        out_text = hgl::serialize::ToTomlText(root);
        return !out_text.IsEmpty();
    }

    bool WriteMaterialDataBlockBinary(const MaterialDataBlock &block,
                                      ValueArray<uint8> &out_bytes) noexcept
    {
        out_bytes.Clear();

        if (!block.IsValid() || !IsContentRowType(block.type))
            return false;
        if (block.row_bytes != RowBytesFor(block.type))
            return false;

        const uint32 row_count = block.GetRowCount();
        const uint32 byte_size = block.GetByteSize();

        if (row_count == 0 || row_count > kMaterialDataBlockMaxRows)
            return false;

        // 定长头 + 行区，小端存放（本项目所有目标平台都是小端，Vulkan 侧同样）。
        if (!out_bytes.Resize(static_cast<int>(kMaterialDataBlockBinaryHeaderSize + byte_size)))
            return false;

        uint8 *dst = out_bytes.GetData();

        dst[0] = 'M';
        dst[1] = 'D';
        dst[2] = 'B';
        dst[3] = '1';
        WriteU16(dst + 4, kMaterialDataBlockBinaryVersion);
        dst[6] = static_cast<uint8>(block.type);
        dst[7] = 0;
        WriteU32(dst + 8, block.row_bytes);
        WriteU32(dst + 12, row_count);

        memcpy(dst + kMaterialDataBlockBinaryHeaderSize, block.GetData(), byte_size);
        return true;
    }

    bool ViewMaterialDataBlockBinary(const uint8 *data,
                                     const uint64 size,
                                     MaterialDataBlockView &out_view,
                                     AnsiString &out_error) noexcept
    {
        out_view = MaterialDataBlockView{};
        out_error.Clear();

        if (!data || size < kMaterialDataBlockBinaryHeaderSize)
        {
            out_error = "数据不足一个块头";
            return false;
        }

        if (data[0] != 'M' || data[1] != 'D' || data[2] != 'B' || data[3] != '1')
        {
            out_error = "块头 magic 不是 MDB1";
            return false;
        }

        const uint16 version = ReadU16(data + 4);
        if (version != kMaterialDataBlockBinaryVersion)
        {
            out_error = "块头 version 不受支持";
            return false;
        }

        const uint32 row_type = data[6];
        if (row_type >= GlobalSSBOTypeCount)
        {
            out_error = "块头 row_type 越界";
            return false;
        }

        const auto type = static_cast<GlobalSSBOType>(row_type);
        if (!IsContentRowType(type))
        {
            out_error = "块头 row_type 不是材质内容行";
            return false;
        }

        const uint32 row_bytes = ReadU32(data + 8);
        if (row_bytes != RowBytesFor(type))
        {
            out_error = "块头 row_bytes 与类型的物理行宽不符";
            return false;
        }

        const uint32 row_count = ReadU32(data + 12);
        if (row_count == 0 || row_count > kMaterialDataBlockMaxRows)
        {
            out_error = "块头 row_count 不合法";
            return false;
        }

        const uint64 need = static_cast<uint64>(kMaterialDataBlockBinaryHeaderSize)
                          + static_cast<uint64>(row_bytes) * row_count;
        if (size < need)
        {
            out_error = "行区长度不足（文件被截断？）";
            return false;
        }

        out_view.type      = type;
        out_view.row_bytes = row_bytes;
        out_view.row_count = row_count;
        out_view.rows      = data + kMaterialDataBlockBinaryHeaderSize;
        return true;
    }

    bool ReadMaterialDataBlockBinary(const uint8 *data,
                                     const uint64 size,
                                     MaterialDataBlock &out_block,
                                     AnsiString &out_error) noexcept
    {
        out_block = MaterialDataBlock{};

        MaterialDataBlockView view;
        if (!ViewMaterialDataBlockBinary(data, size, view, out_error))
            return false;

        const uint32 byte_size = view.GetByteSize();

        ValueArray<uint8> payload;
        if (!payload.Resize(static_cast<int>(byte_size)))
        {
            out_error = "分配块 payload 失败";
            return false;
        }

        memcpy(payload.GetData(), view.rows, byte_size);

        out_block.type      = view.type;
        out_block.row_bytes = view.row_bytes;
        out_block.payload   = std::move(payload);
        return true;
    }
}//namespace hgl::graph::mtl
