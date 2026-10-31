#include<hgl/asset/AssetURI.h>
#include<hgl/utf.h>

namespace hgl::asset
{
    const char *GetAssetTypeName(AssetType type)
    {
        for(const AssetTypeDesc &desc:AssetTypeTable)
            if(desc.type==type)
                return desc.name;

        return "unknown";
    }

    const char *GetAssetTypeLabel(AssetType type)
    {
        for(const AssetTypeDesc &desc:AssetTypeTable)
            if(desc.type==type)
                return desc.label;

        return "unknown";
    }

    AssetType ParseAssetType(const char *name)
    {
        if(!name||!*name)
            return AssetType::Unknown;

        for(const AssetTypeDesc &desc:AssetTypeTable)
            if(hgl::stricmp(name,desc.name)==0)
                return desc.type;

        return AssetType::Unknown;
    }

    AssetType ParseAssetType(const AnsiString &name)
    {
        if(name.IsEmpty())
            return AssetType::Unknown;

        return ParseAssetType(name.c_str());
    }

    const char *GetAssetSourceKindName(AssetSourceKind kind)
    {
        for(const AssetSourceKindDesc &desc:AssetSourceKindTable)
            if(desc.kind==kind)
                return desc.name;

        return "unknown";
    }

    AssetSourceKind ParseAssetSourceKind(const char *name)
    {
        if(!name||!*name)
            return AssetSourceKind::Unknown;

        for(const AssetSourceKindDesc &desc:AssetSourceKindTable)
            if(hgl::stricmp(name,desc.name)==0)
                return desc.kind;

        return AssetSourceKind::Unknown;
    }

    AssetSourceKind ParseAssetSourceKind(const AnsiString &name)
    {
        if(name.IsEmpty())
            return AssetSourceKind::Unknown;

        return ParseAssetSourceKind(name.c_str());
    }

    /// 类型段最短长度。卡长度下限是为了让 `C:\data\x.geo.toml` 这类盘符
    /// 天然落到「不是类型段」那一支，而不是被解析成 type="C"。
    inline constexpr int kAssetURITypeTokenMinLength = 2;

    /// 类型段最长长度（纯防御：真类型名不该超过这个长度）
    inline constexpr int kAssetURITypeTokenMaxLength = 16;

    namespace
    {
        /// 把 URI 的一个短段（ASCII 标识符）从 OSString 转成 AnsiString
        AnsiString SegmentToAnsi(const OSString &segment)
        {
            if(segment.IsEmpty())
                return AnsiString();

            const U8String u8_seg=ToU8String(segment);

            return AnsiString(reinterpret_cast<const char *>(u8_seg.c_str()),u8_seg.Length());
        }

        /// 从 start 起找第一个冒号，找不到返回 -1
        int FindColon(const OSString &str,int start)
        {
            const os_char *data=str.c_str();
            const int      len =str.Length();

            for(int i=start;i<len;i++)
                if(data[i]==OS_TEXT(':'))
                    return i;

            return -1;
        }

        bool IsIdentChar(const os_char ch)
        {
            if(ch>=OS_TEXT('a')&&ch<=OS_TEXT('z'))return true;
            if(ch>=OS_TEXT('A')&&ch<=OS_TEXT('Z'))return true;
            if(ch>=OS_TEXT('0')&&ch<=OS_TEXT('9'))return true;
            if(ch==OS_TEXT('_')||ch==OS_TEXT('-'))return true;

            return false;
        }

        /**
         * 这一段长得像不像一个资产类型标识符。
         *
         * 判据刻意保守：只认 [A-Za-z0-9_-]，长度 2..16。
         */
        bool LooksLikeTypeToken(const OSString &segment)
        {
            const int len=segment.Length();

            if(len<kAssetURITypeTokenMinLength||len>kAssetURITypeTokenMaxLength)
                return false;

            const os_char *data=segment.c_str();

            for(int i=0;i<len;i++)
                if(!IsIdentChar(data[i]))
                    return false;

            return true;
        }
    }//namespace

    AnsiString AssetURISegmentToAnsi(const OSString &segment)
    {
        return SegmentToAnsi(segment);
    }

    OSString AssetURI::MakeAssetURI(AssetType type,AssetSourceKind source,const OSString &target)
    {
        OSString result;

        if(type!=AssetType::Unknown)
        {
            result=ToOSString(GetAssetTypeName(type));
            result+=OS_TEXT(":");
        }

        if(source!=AssetSourceKind::Unknown)
        {
            result+=ToOSString(GetAssetSourceKindName(source));
            result+=OS_TEXT(":");
        }

        result+=target;

        return result;
    }

    AssetURI AssetURI::Parse(const OSString &uri,AssetType default_type)
    {
        AssetURI info;

        info.raw=uri;

        if(default_type!=AssetType::Unknown)
        {
            info.type     =default_type;
            info.type_name=GetAssetTypeName(default_type);
        }

        if(uri.IsEmpty())
            return info;

        const int first=FindColon(uri,0);

        if(first<0)
        {
            info.target=uri;
            return info;
        }

        const OSString   head     =uri.SubString(0,first);
        const AnsiString head_ansi=SegmentToAnsi(head);

        // ── 第一段是已注册的资产类型（geo: / tex: ...）────────────────────
        const AssetType head_type=ParseAssetType(head_ansi);

        if(head_type!=AssetType::Unknown)
        {
            info.type     =head_type;
            info.type_name=GetAssetTypeName(head_type);

            const OSString rest  =uri.SubString(first+1);
            const int      second=FindColon(rest,0);

            if(second>0)
            {
                const AnsiString      source_ansi=SegmentToAnsi(rest.SubString(0,second));
                const AssetSourceKind source_kind=ParseAssetSourceKind(source_ansi);

                if(source_kind!=AssetSourceKind::Unknown)
                {
                    info.source     =source_kind;
                    info.source_name=source_ansi;
                    info.target     =rest.SubString(second+1);
                    return info;
                }
            }

            // 没有可识别的来源段（geo:cube / geo:@cube）：整段当作 target
            info.target=rest;
            return info;
        }

        // ── 第一段是来源而不是类型（file: / rt: / builtin:）────────────────
        // 这类文本在旧代码里就是纹理 URI；这里不改写类型，只把来源与定位段
        // 拆开，类型交给调用方的 default_type。
        const AssetSourceKind head_source=ParseAssetSourceKind(head_ansi);

        if(head_source!=AssetSourceKind::Unknown)
        {
            info.source     =head_source;
            info.source_name=head_ansi;
            info.target     =uri.SubString(first+1);
            return info;
        }

        // ── 第一段长得像类型但没注册（foo:file:bar.dat）──────────────────────
        // 这是「未来类型」的正常输入，必须能解析出来供工厂判定降级。
        if(LooksLikeTypeToken(head))
        {
            info.type_name=head_ansi;      // 原拼写留档；type 保持 Unknown

            const OSString rest  =uri.SubString(first+1);
            const int      second=FindColon(rest,0);

            if(second>0)
            {
                const AnsiString      source_ansi=SegmentToAnsi(rest.SubString(0,second));
                const AssetSourceKind source_kind=ParseAssetSourceKind(source_ansi);

                if(source_kind!=AssetSourceKind::Unknown)
                {
                    info.source     =source_kind;
                    info.source_name=source_ansi;
                    info.target     =rest.SubString(second+1);
                    return info;
                }
            }

            info.target=rest;
            return info;
        }

        // ── 其它（盘符路径、带冒号的普通文件名）：整串当定位串 ───────────────
        info.target=uri;
        return info;
    }

    OSString AssetURI::ToString()const
    {
        const bool has_type_segment  =(type!=AssetType::Unknown||!type_name.IsEmpty());
        const bool has_source_segment=(source!=AssetSourceKind::Unknown);

        if(!has_type_segment&&!has_source_segment)
            return target;

        OSString result;

        if(has_type_segment)
        {
            result=ToOSString(type_name);
            result+=OS_TEXT(":");
        }

        if(has_source_segment)
        {
            if(source_name.IsEmpty())
                result+=ToOSString(GetAssetSourceKindName(source));
            else
                result+=ToOSString(source_name);

            result+=OS_TEXT(":");
        }

        result+=target;

        return result;
    }

    OSString AssetURI::Normalize(const OSString &uri,AssetType default_type)
    {
        return Parse(uri,default_type).ToString();
    }
}//namespace hgl::asset