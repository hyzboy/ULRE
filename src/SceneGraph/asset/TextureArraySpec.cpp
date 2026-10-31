#include<hgl/graph/asset/TextureArraySpec.h>

#include<hgl/filesystem/FileSystem.h>
#include<hgl/filesystem/Path.h>
#include<hgl/graph/asset/TextureURI.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/serialize/TomlCodec.h>
#include<hgl/serialize/Value.h>
#include<hgl/type/Smart.h>
#include<hgl/utf.h>

#include<climits>

namespace hgl::graph
{
    namespace
    {
        /// `U8String`（TOML 字符串）→ `AnsiString`（内部标识符）
        AnsiString ToAnsi(const U8String &text)
        {
            if(text.IsEmpty())
                return AnsiString();

            return AnsiString(reinterpret_cast<const char *>(text.c_str()),text.Length());
        }

        AnsiString MakeField(const char *head,int index)
        {
            AnsiString field(head);

            field+="[";
            field+=AnsiString::numberOf(static_cast<uint>(index));
            field+="]";

            return field;
        }

        AnsiString MakeField(const char *head,int index,const char *tail)
        {
            AnsiString field=MakeField(head,index);

            field+=".";
            field+=tail;

            return field;
        }

        bool ReadWholeFile(const OSString &path,AutoDeleteArray<uint8> &out_buffer,int64 &out_size)
        {
            out_size=0;

            hgl::io::OpenFileInputStream opener(path);

            if(!opener)
                return(false);

            const int64 size=opener->GetSize();

            if(size<=0||size>INT_MAX)
                return(false);

            if(!out_buffer.alloc(static_cast<size_t>(size)+1))
                return(false);

            if(opener->Read(out_buffer.data(),size)!=size)
                return(false);

            out_buffer[static_cast<size_t>(size)]=0;
            out_size=size;

            return(true);
        }

        /// 层名缺省值：父目录名 + 文件名主干（`.../Concrete_Plain/baseColor.Tex2D` → `Concrete_Plain/baseColor`）。
        ///
        /// 带上父目录是因为同一份多层规约里各层往往**同名不同目录**（每个文件夹都是
        /// `baseColor` / `normal`），只用主干会立刻撞上"层名重复"。目录层级只是命名，
        /// 不参与查找，所以用 `/` 连接是安全的。
        AnsiString MakeLayerNameFromURI(const OSString &uri)
        {
            const TextureURI parsed=TextureURI::Parse(uri);

            if(!parsed.IsValid())
                return AnsiString();

            const OSString filename=hgl::filesystem::GetFilename(parsed.path);
            const OSString stem=hgl::filesystem::GetStem(filename);

            const OSString parent=hgl::filesystem::GetParentPath(parsed.path,false);

            /// `GetParentPath` 在没有分隔符时原样返回输入，靠长度判断是否真取到了父目录
            const OSString folder=(parent.Length()<parsed.path.Length())
                                    ? hgl::filesystem::GetLastPathComponent(parent)
                                    : OSString();

            if(folder.IsEmpty())
                return ToAnsi(ToU8String(stem));

            return ToAnsi(ToU8String(folder)) + "/" + ToAnsi(ToU8String(stem));
        }

        bool IsAllowedRootKey(const AnsiString &key)
        {
            return key==AnsiString("schema")
                || key==AnsiString("name")
                || key==AnsiString("mipmaps")
                || key==AnsiString("layers");
        }

        bool IsAllowedLayerKey(const AnsiString &key)
        {
            return key==AnsiString("name")
                || key==AnsiString("uri");
        }

        bool Fail(TextureArraySpecParseError *out_error,
                  TextureArraySpecParseResult &result,
                  TextureArraySpecParseResult code,
                  const AnsiString &field,
                  const AnsiString &detail)
        {
            result=code;

            if(out_error)
            {
                out_error->field =field;
                out_error->detail=detail;
            }

            return(false);
        }
    }//namespace

    AnsiString TextureArraySpecParseError::Format() const
    {
        AnsiString text;

        if(line>0)
        {
            text+="line ";
            text+=AnsiString::numberOf(static_cast<uint>(line));
            text+=": ";
        }

        if(!field.IsEmpty())
        {
            text+=field;
            text+=": ";
        }

        text+=detail;

        return text;
    }

    const char *GetTextureArraySpecParseResultName(TextureArraySpecParseResult result)
    {
        switch(result)
        {
            case TextureArraySpecParseResult::OK:            return "ok";
            case TextureArraySpecParseResult::Skipped:       return "skipped";
            case TextureArraySpecParseResult::IoError:       return "io_error";
            case TextureArraySpecParseResult::InvalidValue:  return "invalid_value";
            case TextureArraySpecParseResult::UnknownKey:    return "unknown_key";
            default:                                        return "unknown";
        }
    }

    AnsiString MakeTextureArrayNameFromPath(const OSString &path)
    {
        OSString filename=hgl::filesystem::GetFilename(path);

        // 去掉两层扩展名（`.texarray.toml`）；`GetStem` 只砍最后一个 `.`，
        // 直接套用会留下 `.texarray`
        const OSString ext(kTextureArraySpecExtension);

        if(!filename.IsEmpty() && filename.EndsWithIgnoreCase(ext))
            filename=filename.SubString(0,filename.Length()-ext.Length());
        else
            filename=hgl::filesystem::GetStem(filename);

        return ToAnsi(ToU8String(filename));
    }

    OSString ResolveTextureArraySpecPath(const TextureURI &uri)
    {
        if(uri.IsArrayByName())
            return OSString(kTextureArrayDefaultDirectory)
                  +uri.GetArrayName()
                  +kTextureArraySpecExtension;

        return uri.GetArraySpecPath();
    }

    TextureArraySpecParseResult ParseTextureArraySpecText(const char *content,
                                                          const int content_size,
                                                          TextureArraySpec &out,
                                                          TextureArraySpecParseError *out_error)
    {
        out=TextureArraySpec{};

        if(out_error)
            *out_error=TextureArraySpecParseError{};

        if(!content||content_size<=0)
            return TextureArraySpecParseResult::Skipped;

        hgl::serialize::Value root;
        hgl::serialize::TomlParseError toml_error;

        if(!hgl::serialize::FromTomlText(content,static_cast<size_t>(content_size),root,&toml_error))
        {
            if(out_error)
            {
                out_error->line  =toml_error.line;
                out_error->detail=toml_error.detail.IsEmpty()?toml_error.title:toml_error.detail;
            }

            return TextureArraySpecParseResult::InvalidValue;
        }

        TextureArraySpecParseResult result=TextureArraySpecParseResult::OK;

        if(!root.IsTable())
        {
            Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                 AnsiString(),"根节点必须是表");
            return result;
        }

        // ── 严格键校验：未知键一律报错，避免拼错键名后静默走默认值 ──────────
        for(int i=0;i<root.GetFieldCount();i++)
        {
            const char *key=root.GetFieldName(i);

            if(!key)
                continue;

            const AnsiString key_name(key);

            if(!IsAllowedRootKey(key_name))
            {
                Fail(out_error,result,TextureArraySpecParseResult::UnknownKey,
                     key_name,"规约里没有这个键");
                return result;
            }
        }

        // ── schema ────────────────────────────────────────────────────────
        const hgl::serialize::Value *schema_node=root.FindFieldExact("schema");
        int64_t schema_value=0;

        if(!schema_node||!schema_node->GetInt(schema_value))
        {
            Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                 AnsiString("schema"),"必须存在且是整数");
            return result;
        }

        if(schema_value!=kTextureArraySpecSchema)
        {
            Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                 AnsiString("schema"),
                 AnsiString("版本不支持（本引擎只认 ")
                     +AnsiString::numberOf(static_cast<uint>(kTextureArraySpecSchema))
                     +"）");
            return result;
        }

        // ── name ──────────────────────────────────────────────────────────
        const hgl::serialize::Value *name_node=root.FindFieldExact("name");

        if(!name_node||!name_node->IsString()||name_node->GetString().IsEmpty())
        {
            Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                 AnsiString("name"),"必须是非空字符串");
            return result;
        }

        out.name=ToAnsi(name_node->GetString());

        // ── mipmaps（可选，0 = 由第 0 层探针决定）────────────────────────────
        if(const hgl::serialize::Value *mip_node=root.FindFieldExact("mipmaps"))
        {
            int64_t mip_value=0;

            if(!mip_node->GetInt(mip_value)||mip_value<0||mip_value>32)
            {
                Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                     AnsiString("mipmaps"),"必须是 0..32 的整数（0 = 由首层决定）");
                return result;
            }

            out.mipmaps=static_cast<uint32_t>(mip_value);
        }

        // ── layers ────────────────────────────────────────────────────────
        const hgl::serialize::Value *layers_node=root.FindFieldExact("layers");

        if(!layers_node||!layers_node->IsArray())
        {
            Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                 AnsiString("layers"),"必须存在且是数组（写成 [[layers]]）");
            return result;
        }

        const int layer_count=layers_node->GetElementCount();

        if(layer_count<=0)
        {
            Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                 AnsiString("layers"),"至少要有一层");
            return result;
        }

        if(layer_count>kTextureArrayMaxLayers)
        {
            Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                 AnsiString("layers"),
                 AnsiString("层数超过上限 ")+AnsiString::numberOf(static_cast<uint>(kTextureArrayMaxLayers)));
            return result;
        }

        for(int i=0;i<layer_count;i++)
        {
            const hgl::serialize::Value *layer_node=layers_node->GetElement(i);

            if(!layer_node||!layer_node->IsTable())
            {
                Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                     MakeField("layers",i),"必须是表（写成 [[layers]]）");
                return result;
            }

            for(int k=0;k<layer_node->GetFieldCount();k++)
            {
                const char *key=layer_node->GetFieldName(k);

                if(!key)
                    continue;

                const AnsiString key_name(key);

                if(!IsAllowedLayerKey(key_name))
                {
                    Fail(out_error,result,TextureArraySpecParseResult::UnknownKey,
                         MakeField("layers",i,key_name.c_str()),
                         "规约里没有这个键");
                    return result;
                }
            }

            TextureArrayLayerSpec layer;

            const hgl::serialize::Value *uri_node=layer_node->FindFieldExact("uri");

            if(!uri_node||!uri_node->IsString()||uri_node->GetString().IsEmpty())
            {
                Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                     MakeField("layers",i,"uri"),"必须是非空字符串");
                return result;
            }

            layer.uri=ToOSString(uri_node->GetString());

            const TextureURI parsed=TextureURI::Parse(layer.uri);

            if(!parsed.IsValid())
            {
                Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                     MakeField("layers",i,"uri"),"不是可用的纹理 URI");
                return result;
            }

            if(parsed.IsRenderTarget())
            {
                // 数组层是**建数组时一次性上传**的磁盘数据；RT 附件是另一张随时重建的纹理，
                // 「把 RT 拷进数组某层」需要每帧回读，不属于层装配语义。
                Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                     MakeField("layers",i,"uri"),"数组层不支持 rt: 来源（必须是磁盘纹理）");
                return result;
            }

            if(parsed.IsTextureArray())
            {
                Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                     MakeField("layers",i,"uri"),"数组层不能又是另一个数组");
                return result;
            }

            if(const hgl::serialize::Value *layer_name_node=layer_node->FindFieldExact("name"))
            {
                if(!layer_name_node->IsString()||layer_name_node->GetString().IsEmpty())
                {
                    Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                         MakeField("layers",i,"name"),"写了就必须是非空字符串");
                    return result;
                }

                layer.name=ToAnsi(layer_name_node->GetString());
            }
            else
                layer.name=MakeLayerNameFromURI(layer.uri);

            if(layer.name.IsEmpty())
            {
                Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                     MakeField("layers",i,"name"),"无法从 URI 推出层名，请显式写 name");
                return result;
            }

            for(int j=0;j<out.layers.GetCount();j++)
            {
                if(out.layers[j]->name==layer.name)
                {
                    Fail(out_error,result,TextureArraySpecParseResult::InvalidValue,
                         MakeField("layers",i,"name"),
                         AnsiString("层名重复（与第 ")
                             +AnsiString::numberOf(static_cast<uint>(j))
                             +" 层同名）："
                             +layer.name);
                    return result;
                }
            }

            // `ManagedArray` 拥有指针（与 SceneAssetSpec/MaterialRecipeSpec 同一套约定）
            out.layers.Add(new TextureArrayLayerSpec(layer));
        }

        return TextureArraySpecParseResult::OK;
    }

    TextureArraySpecParseResult ParseTextureArraySpecFile(const OSString &path,
                                                          TextureArraySpec &out,
                                                          TextureArraySpecParseError *out_error)
    {
        out=TextureArraySpec{};

        if(out_error)
            *out_error=TextureArraySpecParseError{};

        if(path.IsEmpty())
        {
            if(out_error)
                out_error->detail="路径为空";

            return TextureArraySpecParseResult::IoError;
        }

        AutoDeleteArray<uint8> buffer;
        int64 size=0;

        if(!ReadWholeFile(path,buffer,size))
        {
            if(out_error)
                out_error->detail=AnsiString("文件读不出来：")+ToAnsi(ToU8String(path));

            return TextureArraySpecParseResult::IoError;
        }

        return ParseTextureArraySpecText(reinterpret_cast<const char *>(buffer.data()),
                                         static_cast<int>(size),out,out_error);
    }
}//namespace hgl::graph
