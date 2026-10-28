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

        // 无已知 scheme：按文件路径处理
        info.kind=TextureURIKind::File;
        info.path=uri;

        return(info);
    }

    OSString TextureURI::ToString()const
    {
        if(kind==TextureURIKind::RenderTarget)
            return OSString(OS_TEXT("rt:"))+path+OSString(OS_TEXT(":"))+attachment;

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
