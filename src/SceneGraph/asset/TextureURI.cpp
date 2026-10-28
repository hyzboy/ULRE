#include<hgl/graph/asset/TextureURI.h>

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

            int colon=-1;
            for(int i=0;i<len;i++)
                if(data[i]==OS_TEXT(':'))
                {
                    colon=i;
                    break;
                }

            if(colon>=0)
            {
                info.path      =rest.SubString(0,colon);
                info.attachment=rest.SubString(colon+1);
            }
            else
            {
                info.path=rest;
            }

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
}//namespace hgl::graph
