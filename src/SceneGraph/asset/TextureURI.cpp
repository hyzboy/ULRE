#include<hgl/graph/asset/TextureURI.h>
#include<hgl/graph/asset/RenderTargetAsset.h>
#include<hgl/utf.h>

namespace hgl::graph
{
    namespace
    {
        os_char LowerChar(const os_char ch)
        {
            if(ch>=OS_TEXT('A')&&ch<=OS_TEXT('Z'))
                return os_char(ch-OS_TEXT('A')+OS_TEXT('a'));

            return ch;
        }

        /**
         * 前缀匹配（ASCII 大小写不敏感）。
         *
         * 用固定前缀而不是「扫描到冒号再判 scheme」，是为了让 `C:\textures\a.Tex2D`
         * 这类带盘符的路径天然落进 File 分支，不需要额外的盘符特判。
         */
        bool HasPrefixNoCase(const OSString &str,const os_char *prefix)
        {
            const int len=str.Length();

            int prefix_len=0;
            while(prefix[prefix_len])
                ++prefix_len;

            if(len<prefix_len)
                return(false);

            const os_char *data=str.c_str();

            for(int i=0;i<prefix_len;i++)
                if(LowerChar(data[i])!=LowerChar(prefix[i]))
                    return(false);

            return(true);
        }

        bool HasSuffixNoCase(const OSString &str,const os_char *suffix)
        {
            int suffix_len=0;
            while(suffix[suffix_len])
                ++suffix_len;

            const int len=str.Length();
            if(len<suffix_len)
                return(false);

            const os_char *data=str.c_str()+len-suffix_len;
            for(int i=0;i<suffix_len;i++)
                if(LowerChar(data[i])!=LowerChar(suffix[i]))
                    return(false);

            return(true);
        }

        /**
         * 这段文本是不是一个合法的附件名（`color` / `color<N>` / `depth`）。
         *
         * 用来决定 `rt:` 里最后一个冒号是「名字与附件的分隔符」还是「名字自身的一部分」：
         * RT 名常常沿用 `resource_prefix`（形如 `RenderToTexture:OffscreenRT`），
         * 不能简单地按第一个或最后一个冒号切分。
         */
        bool IsAttachmentName(const OSString &name)
        {
            if(name.IsEmpty())
                return(false);

            if(name==OSString(kTextureURIAttachmentDepth))
                return(true);

            uint32_t index=0;
            return(ParseColorAttachmentName(name,index));
        }

        /**
         * 识别 `pack(<name>):<target>` 语法（`AssetSourceMount::PackAssetURI` 的产出形态）。
         *
         * 合法条件：`pack(` 前缀（ASCII 大小写不敏感）、名字非空且不含 `)`/`:`、
         * 有 `):` 定界符、其后的目标非空。
         *
         * 匹配成功时 `out_name_end` 指向 `)`，`out_target_begin` 指向目标首字符。
         * **不**检查包裹是否已挂载：解析保持纯语法，挂载状态由取流/解析层裁决，
         * 离线配方校验这类无挂载环境才能正常解析文本。
         */
        bool TryParsePack(const OSString &text,int &out_name_end,int &out_target_begin)
        {
            out_name_end      =-1;
            out_target_begin  =-1;

            if(!HasPrefixNoCase(text,kTextureURIPackPrefix))
                return(false);

            const os_char *data=text.c_str();
            const int      len =text.Length();

            int i=5;      // 跳过 "pack("
            while(i<len && data[i]!=OS_TEXT(')'))
            {
                if(data[i]==OS_TEXT(':'))
                    return(false);
                ++i;
            }

            // 必须是 `):` 收尾，且名字（i-5）与目标（len-i-2）都非空
            if(data[i]!=OS_TEXT(')') || i+1>=len || data[i+1]!=OS_TEXT(':'))
                return(false);

            if(i<=5 || i+2>=len)
                return(false);

            out_name_end     =i;
            out_target_begin =i+2;
            return(true);
        }
    }//namespace

    TextureURI TextureURI::Parse(const OSString &uri)
    {
        TextureURI info;

        info.raw=uri;

        if(uri.IsEmpty())
            return(info);

        if(HasPrefixNoCase(uri,OS_TEXT("file:")))
        {
            info.kind=TextureURIKind::File;
            info.path=uri.SubString(5);      // 跳过 "file:"
            return(info);
        }

        // 必须在 `file:` 之后判：`cubefile:` 不以 `file:` 开头，顺序其实无关，
        // 但保持「2D 文件优先」的读法更省心。
        if(HasPrefixNoCase(uri,OS_TEXT("cubefile:")))
        {
            info.kind=TextureURIKind::CubeFile;
            info.path=uri.SubString(9);      // 跳过 "cubefile:"
            return(info);
        }

        // ── 类型前缀写法（与 `geo:` 系列对齐；目标可不带容器扩展名）──
        if(HasPrefixNoCase(uri,OS_TEXT("tex2d:")))
        {
            info.kind=TextureURIKind::File;
            info.path=uri.SubString(6);      // 跳过 "tex2d:"
            return(info);
        }

        if(HasPrefixNoCase(uri,OS_TEXT("texcube:")))
        {
            info.kind=TextureURIKind::CubeFile;
            info.path=uri.SubString(8);      // 跳过 "texcube:"
            return(info);
        }

        if(HasPrefixNoCase(uri,OS_TEXT("texoct:")))
        {
            info.kind=TextureURIKind::OctaFile;
            info.path=uri.SubString(7);      // 跳过 "texoct:"
            return(info);
        }

        if(HasPrefixNoCase(uri,OS_TEXT("rt:")))
        {
            info.kind=TextureURIKind::RenderTarget;

            const OSString rest=uri.SubString(3);

            const os_char *data=rest.c_str();
            const int len=rest.Length();

            // 从**尾部**找最后一个冒号，并只看最后一小段是否是合法附件名。
            // RT 名可能自带冒号（示例的 resource_prefix 就是 `Xxx:Yyy`），
            // 而附件名只有 `color<N>`/`depth` 这几种，因此可以无歧义地区分：
            //   rt:RenderToTexture:OffscreenRT:color0 → 名 `RenderToTexture:OffscreenRT`，附件 `color0`
            //   rt:RenderToTexture:OffscreenRT        → 整段是名字，附件缺省 `color0`
            int colon=-1;
            for(int i=len-1;i>=0;--i)
                if(data[i]==OS_TEXT(':'))
                {
                    const OSString tail=rest.SubString(i+1);

                    if(IsAttachmentName(tail))
                    {
                        colon=i;
                        info.attachment=tail;
                    }

                    break;
                }

            if(colon>=0)
                info.path=rest.SubString(0,colon);
            else
                info.path=rest;

            if(info.attachment.IsEmpty())
                info.attachment=kTextureURIAttachmentColor;

            return(info);
        }

        if(HasPrefixNoCase(uri,OS_TEXT("texarray:")))
        {
            info.kind=TextureURIKind::TextureArray;

            const OSString rest=uri.SubString(9);      // 跳过 "texarray:"

            // 只接受 `@<name>` 或 `file:<path>`；其余形态留空 → !IsValid()
            if(!rest.IsEmpty()
             &&(rest.c_str()[0]==kTextureURIArrayNamePrefix
                ||HasPrefixNoCase(rest,kTextureURIArrayFilePrefix)))
                info.path=rest;

            return(info);
        }

        // `pack(<name>):<target>`：包裹内寻址。仍是 File 形态（包裹根背后就是
        // 文件树），但 path 保留**完整原文**，交给 ResolveAssetPhysicalPath /
        // AssetURIByteSource 按包裹解析；避免 ToString() 拼出 `file:pack(...)`。
        {
            int name_end,target_begin;

            if(TryParsePack(uri,name_end,target_begin))
            {
                info.kind=TextureURIKind::File;
                info.path=uri;
                return(info);
            }

            if(HasPrefixNoCase(uri,kTextureURIPackPrefix))
            {
                // `pack(` 开头却不合法（缺 `):`、名字或目标为空）：按非法处理，
                // 不能让坏包裹 URI 悄悄当成磁盘路径去开文件。
                return(info);
            }
        }

        // 无已知 scheme：按文件路径处理
        info.kind=TextureURIKind::File;
        info.path=uri;

        return(info);
    }

    bool TextureURI::IsArrayByName()const
    {
        return kind==TextureURIKind::TextureArray
            && !path.IsEmpty()
            && path.c_str()[0]==kTextureURIArrayNamePrefix;
    }

    bool TextureURI::IsPackURI()const
    {
        if(!IsDiskFile())
            return(false);

        int name_end,target_begin;
        return(TryParsePack(path,name_end,target_begin));
    }

    bool TextureURI::IsOctahedralFile()const
    {
        if(kind==TextureURIKind::OctaFile)
            return(true);

        // 旧写法的兼容路径：`file:`/裸路径 + `.TexOct` 后缀。
        // `cubefile:`/`texcube:` 即使带该后缀也不算八面体（形态由前缀定死）。
        return kind==TextureURIKind::File
            && HasSuffixNoCase(path,OS_TEXT(".TexOct"));
    }

    OSString TextureURI::GetArrayName()const
    {
        if(!IsArrayByName())
            return OSString();

        return path.SubString(1);
    }

    OSString TextureURI::GetArraySpecPath()const
    {
        if(kind!=TextureURIKind::TextureArray)
            return OSString();

        if(HasPrefixNoCase(path,kTextureURIArrayFilePrefix))
            return path.SubString(5);

        return OSString();
    }

    OSString TextureURI::GetPackName()const
    {
        if(!IsDiskFile())
            return OSString();

        int name_end,target_begin;
        if(!TryParsePack(path,name_end,target_begin))
            return OSString();

        return path.SubString(5,name_end-5);
    }

    OSString TextureURI::GetPackTarget()const
    {
        if(!IsDiskFile())
            return OSString();

        int name_end,target_begin;
        if(!TryParsePack(path,name_end,target_begin))
            return OSString();

        return path.SubString(target_begin);
    }

    OSString TextureURI::ToString()const
    {
        if(kind==TextureURIKind::RenderTarget)
            return OSString(OS_TEXT("rt:"))+path+OSString(OS_TEXT(":"))+attachment;

        if(kind==TextureURIKind::TextureArray)
            return OSString(OS_TEXT("texarray:"))+path;

        if(kind==TextureURIKind::OctaFile)
            return OSString(OS_TEXT("texoct:"))+path;

        if(kind==TextureURIKind::CubeFile)
            return OSString(OS_TEXT("cubefile:"))+path;

        // 包裹原文本身就是完整 URI，再补 `file:` 会拼出双重前缀
        if(IsPackURI())
            return path;

        return OSString(OS_TEXT("file:"))+path;
    }

    OSString TextureURI::Normalize(const OSString &uri)
    {
        return Parse(uri).ToString();
    }

    AnsiString ToRenderTargetName(const OSString &os_name)
    {
        if(os_name.IsEmpty())
            return(AnsiString());

        const U8String u8_name=ToU8String(os_name);

        return AnsiString(reinterpret_cast<const char *>(u8_name.c_str()),u8_name.Length());
    }

    OSString FromRenderTargetName(const AnsiString &ansi_name)
    {
        if(ansi_name.IsEmpty())
            return(OSString());

        return ToOSString(U8String(reinterpret_cast<const u8char *>(ansi_name.c_str()),ansi_name.Length()));
    }
}//namespace hgl::graph
