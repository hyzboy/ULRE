#include <hgl/graph/asset/EnvironmentAsset.h>
#include <hgl/asset/generated/EnvironmentMeta.gen.h>
#include <hgl/utf.h>

namespace hgl::graph
{
    namespace
    {
        /// UTF-8 字节串 ⇒ `U8String`（`HGL_SUPPORT_CHAR8_T` 未开时没有现成转换）
        U8String ToU8(const AnsiString &str)
        {
            return U8String(reinterpret_cast<const u8char *>(str.c_str()), str.Length());
        }

        /// `OSString` ⇒ UTF-8 字节串（`ToU8String` 给的是 `U8String`，这里要 `AnsiString`）
        AnsiString ToAnsi(const OSString &str)
        {
            const U8String u8 = ToU8String(str);
            return AnsiString(reinterpret_cast<const char *>(u8.c_str()), u8.Length());
        }
    }//namespace

    EnvironmentAsset::EnvironmentAsset(EnvironmentSpec &&in_spec)
        : spec(std::move(in_spec))
    {
        asset_name = spec.name;
    }

    const hgl::asset::ClassMetaInfo *EnvironmentAsset::GetClassMetaInfo() const
    {
        return &hgl::asset::generated::Class_EnvironmentSpec;
    }

    void EnvironmentAsset::SetSource(const os_char *label)
    {
        source_path = label ? label : OS_TEXT("");

        // 身份按「有名字优先」生成：名字是作者给的稳定键，路径只是它被发现的位置。
        // 两者都保留，因为名字查询命中后仍需知道原始文件（热重载 / 报错定位）。
        if (spec.name.IsEmpty())
            resource_id = ToU8(AnsiString("env:file:") + ToAnsi(source_path));
        else
            resource_id = ToU8(AnsiString("env:@") + spec.name);

        if (asset_name.IsEmpty())
            asset_name = source_path.IsEmpty() ? AnsiString("<memory>") : ToAnsi(source_path);
    }
}//namespace hgl::graph
