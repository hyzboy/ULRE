#include<hgl/graph/asset/RenderTargetAsset.h>
#include<hgl/graph/asset/TextureURI.h>

#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKTexture.h>

#include<hgl/log/Log.h>

#include<hgl/filesystem/FileSystem.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/type/Smart.h>
#include<hgl/utf.h>

#include<toml/toml.hpp>

#include<string>
#include<cstring>

namespace hgl::graph
{
    namespace
    {
        struct FormatText
        {
            VkFormat    format;
            const char *text;
        };

        /// 格式名表：TOML 里只暴露**常用且可采样/渲染**的格式，拼写即唯一真源。
        constexpr FormatText kFormatText[]=
        {
            { PF_UNDEFINED,              "undefined"     },
            { PF_RGBA8UN,                "rgba8"         },
            { PF_RGBA8s,                 "rgba8_srgb"    },
            { VK_FORMAT_B8G8R8A8_UNORM,  "bgra8"         },
            { VK_FORMAT_B8G8R8A8_SRGB,   "bgra8_srgb"    },
            { PF_RGBA16F,                "rgba16f"       },
            { VK_FORMAT_R16G16_SFLOAT,   "rg16f"         },
            { VK_FORMAT_R16_SFLOAT,      "r16f"          },
            { PF_R32F,                   "r32f"          },
            { VK_FORMAT_R32G32_SFLOAT,   "rg32f"         },
            { PF_RGBA32F,                "rgba32f"       },
            { PF_R8UN,                   "r8"            },
            { PF_RG8UN,                  "rg8"           },
            { PF_RGBA8U,                 "rgba8_uint"    },
            { PF_D16UN,                  "d16"           },
            { PF_D32F,                   "d32"           },
            { PF_D24UN_S8U,              "d24s8"         },
            { PF_D32F_S8U,               "d32s8"         },
        };
    }//namespace

    const char *GetRenderTargetFormatName(const VkFormat fmt)
    {
        for(const FormatText &item:kFormatText)
            if(item.format==fmt)
                return item.text;

        return(nullptr);
    }

    bool ParseRenderTargetFormat(const char *text,VkFormat &out_fmt)
    {
        if(!text||!*text)
            return(false);

        for(const FormatText &item:kFormatText)
            if(std::strcmp(item.text,text)==0)
            {
                out_fmt=item.format;
                return(true);
            }

        return(false);
    }

    // ── 附件寻址 ────────────────────────────────────────────────────────────

    OSString MakeColorAttachmentName(const uint32_t index)
    {
        return OSString(OS_TEXT("color"))+OSString::numberOf(static_cast<uint>(index));
    }

    bool ParseColorAttachmentName(const OSString &name,uint32_t &out_index)
    {
        if(name.IsEmpty())
            return(false);

        const os_char *str=name.c_str();
        const int len=name.Length();

        int pos=0;

        // "color" 前缀，允许简写成 "color"（等价 color0）
        const os_char *prefix=OS_TEXT("color");
        int prefix_len=0;
        while(prefix[prefix_len])
            ++prefix_len;

        if(len<prefix_len)
            return(false);

        for(int i=0;i<prefix_len;i++)
        {
            os_char ch=str[i];
            if(ch>=OS_TEXT('A')&&ch<=OS_TEXT('Z'))
                ch=os_char(ch-OS_TEXT('A')+OS_TEXT('a'));

            if(ch!=prefix[i])
                return(false);
        }

        pos=prefix_len;

        if(pos==len)
        {
            out_index=0;
            return(true);
        }

        uint32_t index=0;

        for(int i=pos;i<len;i++)
        {
            const os_char ch=str[i];

            if(ch<OS_TEXT('0')||ch>OS_TEXT('9'))
                return(false);

            index=index*10+static_cast<uint32_t>(ch-OS_TEXT('0'));
        }

        out_index=index;

        return(true);
    }

    Texture2D *ResolveRenderTargetAttachment(IRenderTarget *rt,const OSString &attachment,const char **error)
    {
        if(!rt)
        {
            if(error)
                *error="RenderTarget 为空";

            return(nullptr);
        }

        // 缺省附件 = color0
        if(attachment.IsEmpty())
            return(rt->GetColorTexture(0));

        if(attachment==OSString(kTextureURIAttachmentDepth))
        {
            if(!rt->hasDepth())
            {
                if(error)
                    *error="该 RenderTarget 没有深度附件";

                return(nullptr);
            }

            return(rt->GetDepthTexture());
        }

        uint32_t index=0;

        if(!ParseColorAttachmentName(attachment,index))
        {
            if(error)
                *error="无法识别的附件名（合法值：color<N> / depth）";

            return(nullptr);
        }

        if(index>=rt->GetColorCount())
        {
            if(error)
                *error="颜色附件序号越界";

            return(nullptr);
        }

        return(rt->GetColorTexture(static_cast<int>(index)));
    }

    // ── RenderTargetAssetSpec ───────────────────────────────────────────────

    bool RenderTargetAssetSpec::IsValid()const
    {
        if(name.IsEmpty())
            return(false);

        if(width==0||height==0)
            return(false);

        if(size_mode==RenderTargetSizeMode::WindowRelative)
        {
            // 千分比不能过大，否则窗口一变就炸显存
            if(width>8000||height>8000)
                return(false);
        }

        if(!tpl.has_color&&!tpl.has_depth)
            return(false);

        return(true);
    }

    uint32_t RenderTargetAssetSpec::ResolveWidth(const uint32_t window_width)const
    {
        if(size_mode==RenderTargetSizeMode::Absolute)
            return(width);

        const uint32_t w=static_cast<uint32_t>((uint64_t(window_width)*width+500)/1000);

        return(w>0?w:1);
    }

    uint32_t RenderTargetAssetSpec::ResolveHeight(const uint32_t window_height)const
    {
        if(size_mode==RenderTargetSizeMode::Absolute)
            return(height);

        const uint32_t h=static_cast<uint32_t>((uint64_t(window_height)*height+500)/1000);

        return(h>0?h:1);
    }

    uint32_t RenderTargetAssetSpec::GetAttachmentCount()const
    {
        // has_color 为真但 color_formats 为空 ⇒ 创建时补设备默认 surface format，仍是 1 个颜色附件
        const uint32_t color_count = tpl.has_color
                                   ? (tpl.color_formats.empty() ? 1u : tpl.GetColorCount())
                                   : 0u;

        return(color_count+(tpl.has_depth?1u:0u));
    }

    bool RenderTargetAssetSpec::ToDesc(const uint32_t window_width,const uint32_t window_height,RenderTargetDesc &out)const
    {
        if(!IsValid())
            return(false);

        out=tpl;

        out.kind   =RenderTargetKind::Offscreen;
        out.name   =name;
        out.width  =ResolveWidth(window_width);
        out.height =ResolveHeight(window_height);

        return(out.IsValid());
    }

    RenderTargetAssetSpec RenderTargetAssetSpec::ColorDepth(const uint32_t w,const uint32_t h,const AnsiString &name,const VkFormat depth_fmt)
    {
        RenderTargetAssetSpec spec;

        spec.name   =name;
        spec.width  =w;
        spec.height =h;
        spec.tpl    =RenderTargetDesc::OffscreenColorDepth(w,h,name,depth_fmt);

        // 模板里不保留 name/width/height —— 它们在 ToDesc() 里由本 spec 覆写，
        // 留着只会让「以谁为准」变模糊。
        spec.tpl.name  ={};
        spec.tpl.width =0;
        spec.tpl.height=0;

        return(spec);
    }

    RenderTargetAssetSpec RenderTargetAssetSpec::ColorOnly(const uint32_t w,const uint32_t h,const AnsiString &name)
    {
        RenderTargetAssetSpec spec;

        spec.name   =name;
        spec.width  =w;
        spec.height =h;
        spec.tpl    =RenderTargetDesc::OffscreenColorOnly(w,h,name);

        spec.tpl.name  ={};
        spec.tpl.width =0;
        spec.tpl.height=0;

        return(spec);
    }

    RenderTargetAssetSpec RenderTargetAssetSpec::DepthOnly(const uint32_t w,const uint32_t h,const AnsiString &name,const VkFormat depth_fmt)
    {
        RenderTargetAssetSpec spec;

        spec.name   =name;
        spec.width  =w;
        spec.height =h;
        spec.tpl    =RenderTargetDesc::OffscreenDepthOnly(w,h,name,depth_fmt);

        spec.tpl.name  ={};
        spec.tpl.width =0;
        spec.tpl.height=0;

        return(spec);
    }

    // ── TOML ───────────────────────────────────────────────────────────────

    namespace
    {
        bool ReadBool(const toml::value &root,const char *key,const bool def_value)
        {
            if(!root.contains(key))
                return(def_value);

            const toml::value &v=root.at(key);

            if(!v.is_boolean())
                return(def_value);

            return(v.as_boolean());
        }

        uint32_t ReadUInt(const toml::value &root,const char *key,const uint32_t def_value)
        {
            if(!root.contains(key))
                return(def_value);

            const toml::value &v=root.at(key);

            if(!v.is_integer())
                return(def_value);

            const int64_t iv=v.as_integer();

            if(iv<0)
                return(def_value);

            return(static_cast<uint32_t>(iv));
        }

        bool ReadString(const toml::value &root,const char *key,std::string &out)
        {
            if(!root.contains(key))
                return(false);

            const toml::value &v=root.at(key);

            if(!v.is_string())
                return(false);

            out=v.as_string();

            return(true);
        }
    }//namespace

    bool LoadRenderTargetAssetSpecFromTOML(const OSString &filename,RenderTargetAssetSpec &out)
    {
        hgl::io::OpenFileInputStream opener(filename);

        if(!opener)
        {
            GLogError(OS_TEXT("[RenderTargetAsset] 打开失败：%s"),filename.c_str());
            return(false);
        }

        const int64 size=opener->GetSize();

        if(size<=0)
        {
            GLogError(OS_TEXT("[RenderTargetAsset] 文件为空：%s"),filename.c_str());
            return(false);
        }

        hgl::AutoDeleteArray<char> buffer(static_cast<size_t>(size)+1);

        if(!buffer||opener->Read(buffer.data(),size)!=size)
        {
            GLogError(OS_TEXT("[RenderTargetAsset] 读取失败：%s"),filename.c_str());
            return(false);
        }

        buffer[static_cast<size_t>(size)]=0;

        toml::value root;

        try
        {
            root=toml::parse_str(std::string(buffer.data(),static_cast<size_t>(size)));
        }
        catch(const toml::exception &)
        {
            GLogError(OS_TEXT("[RenderTargetAsset] TOML 解析失败：%s"),filename.c_str());
            return(false);
        }

        if(!root.is_table())
        {
            GLogError(OS_TEXT("[RenderTargetAsset] 根节点不是表：%s"),filename.c_str());
            return(false);
        }

        RenderTargetAssetSpec spec;

        std::string name;

        if(ReadString(root,"name",name)&&!name.empty())
            spec.name=AnsiString(name.c_str(),static_cast<int>(name.size()));

        // TOML 里名字可以省略；省略时用文件名（不含扩展名）兜底，保证 rt: 有可寻址的名字。
        if(spec.name.IsEmpty())
        {
            const OSString base=hgl::filesystem::GetStem(filename);

            if(base.IsEmpty())
            {
                GLogError(OS_TEXT("[RenderTargetAsset] 缺少 name：%s"),filename.c_str());
                return(false);
            }

            const U8String u8_base=ToU8String(base);
            spec.name=AnsiString(reinterpret_cast<const char *>(u8_base.c_str()),u8_base.Length());
        }

        std::string size_mode;

        if(ReadString(root,"size_mode",size_mode))
        {
            if(size_mode=="window_relative")
                spec.size_mode=RenderTargetSizeMode::WindowRelative;
            else if(size_mode=="absolute")
                spec.size_mode=RenderTargetSizeMode::Absolute;
            else
            {
                GLogError(OS_TEXT("[RenderTargetAsset] size_mode 非法（%s）：%s"),
                          OS_TEXT("absolute / window_relative"),
                          filename.c_str());
                return(false);
            }
        }

        spec.width =ReadUInt(root,"width" ,0);
        spec.height=ReadUInt(root,"height",0);

        spec.tpl.kind=RenderTargetKind::Offscreen;

        spec.tpl.has_color=ReadBool(root,"has_color",true);
        spec.tpl.has_depth=ReadBool(root,"has_depth",true);

        if(root.contains("color_formats"))
        {
            const toml::value &arr=root.at("color_formats");

            if(!arr.is_array())
            {
                GLogError(OS_TEXT("[RenderTargetAsset] color_formats 必须是数组：%s"),filename.c_str());
                return(false);
            }

            for(const toml::value &item:arr.as_array())
            {
                VkFormat fmt=PF_UNDEFINED;

                if(!item.is_string()||!ParseRenderTargetFormat(item.as_string().c_str(),fmt))
                {
                    GLogError(OS_TEXT("[RenderTargetAsset] color_formats 含未知格式：%s"),filename.c_str());
                    return(false);
                }

                spec.tpl.color_formats.push_back(fmt);
            }
        }

        std::string depth_format;

        if(ReadString(root,"depth_format",depth_format))
        {
            VkFormat fmt=PF_UNDEFINED;

            if(!ParseRenderTargetFormat(depth_format.c_str(),fmt))
            {
                GLogError(OS_TEXT("[RenderTargetAsset] depth_format 未知格式：%s"),filename.c_str());
                return(false);
            }

            spec.tpl.depth_format=fmt;
        }

        spec.tpl.resizable    =ReadBool(root,"resizable"    ,true);
        spec.tpl.follow_window=ReadBool(root,"follow_window",false);

        spec.tpl.slot_count=ReadUInt(root,"slot_count",1);

        if(spec.tpl.slot_count==0)
            spec.tpl.slot_count=1;

        if(root.contains("clear_color"))
        {
            const toml::value &arr=root.at("clear_color");

            if(!arr.is_array()||arr.as_array().size()!=4)
            {
                GLogError(OS_TEXT("[RenderTargetAsset] clear_color 必须是 4 元素数组：%s"),filename.c_str());
                return(false);
            }

            float rgba[4]={0,0,0,1};

            for(size_t i=0;i<4;i++)
            {
                const toml::value &item=arr.as_array()[i];

                if(!item.is_floating()&&!item.is_integer())
                {
                    GLogError(OS_TEXT("[RenderTargetAsset] clear_color 含非数值：%s"),filename.c_str());
                    return(false);
                }

                rgba[i]=item.is_floating()
                       ?static_cast<float>(item.as_floating())
                       :static_cast<float>(item.as_integer());
            }

            spec.tpl.clear_color=Color4f(rgba[0],rgba[1],rgba[2],rgba[3]);
        }

        if(!spec.IsValid())
        {
            GLogError(OS_TEXT("[RenderTargetAsset] 规约非法：%s"),filename.c_str());
            return(false);
        }

        out=spec;

        return(true);
    }
}//namespace hgl::graph
