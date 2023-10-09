#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKShaderStage.h>
#include<hgl/graph/VertexAttrib.h>

#include<hgl/io/TextInputStream.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/filesystem/FileSystem.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    using TextParse=io::TextInputStream::ParseCallback<char>;

    enum class MaterialFileBlock
    {
        None,
        
        Material,
        MaterialInstance,
        VertexInput,

        Vertex,
        Geometry,
        Fragment,

        ENUM_CLASS_RANGE(None,Fragment)
    };//enum class State

    struct MaterialFileBlockInfo
    {
        const char *name;
        const int len;
        MaterialFileBlock state;
    };

    #define MFS(name) {#name,sizeof(#name)-1,MaterialFileBlock::name},

    constexpr const MaterialFileBlockInfo material_file_block_info_list[]=
    {
        MFS(Material)
        MFS(MaterialInstance)
        MFS(VertexInput)
        MFS(Vertex)
        MFS(Geometry)
        MFS(Fragment)
    };

    #undef MFS

    const MaterialFileBlock GetMaterialFileState(const char *str,const int len)
    {
        for(const MaterialFileBlockInfo &info:material_file_block_info_list)
            if(len==info.len)
                if(hgl::stricmp(str,info.name,len)==0)
                    return info.state;

        return MaterialFileBlock::None;
    }

    struct MaterialFileBlockParse:public TextParse
    {
        MaterialFileBlock state;

        MaterialFileBlockParse()
        {
            state=MaterialFileBlock::None;
        }

        bool OnLine(const char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            return(true);    
        }
    };//struct MaterialFileBlockParse

    struct CodeParse:public TextParse
    {
        const char *start   =nullptr;
        const char *end     =nullptr;

    public:

        bool OnLine(const char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            if(*text=='{')
            {
                ++text;
                while(*text=='\r'||*text=='\n')++text;

                start=text;
                return(false);
            }
            
            if(*text=='}')
            {
                end=text;
                return(true);
            }

            return(false);
        }
    };//struct CodeParse

    struct MaterialInstanceStateParse:public TextParse
    {
        bool        code                    =false;
        CodeParse   code_parse;

        uint        mi_bytes                =0;
        uint32_t    shader_stage_flag_bits  =0;

    public:

        bool OnLine(const char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            if(code)
            {
                if(code_parse.OnLine(text,len))
                    code=false;

                return(true);
            }

            if(hgl::stricmp(text,"Code",4)==0)
            {
                code=true;
            }
            else
            if(hgl::stricmp(text,"Length",6)==0)
            {
                text+=7;
                while(*text==' '||*text=='\t')++text;

                hgl::stou(text,mi_bytes);
            }
            else
            if(hgl::stricmp(text,"Stage",5)==0)
            {
                const char *ep=text+len;
                const char *sp;

                text+=6;

                while(*text==' '||*text=='\t')++text;

                while(text<ep)
                {
                    sp=text;

                    while(hgl::isalpha(*text))
                        ++text;

                    shader_stage_flag_bits|=GetShaderStageFlagBits(sp,text-sp);

                    ++text;
                }
            }

            return(true);
        }
    };//struct MaterialInstanceStateParse

    struct UniformAttrib
    {
        VAT vat;
        
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];
    };

    bool ParseUniformAttrib(UniformAttrib *ua,const char *str)
    {
        const char *sp;

        while(*str==' '||*str=='\t')++str;

        if(!ParseVertexAttribType(&(ua->vat),str))
            return(false);

        while(*str!=' '&&*str!='\t')++str;
        while(*str==' '||*str=='\t')++str;

        sp=str;

        while(hgl::iscodechar(*str))++str;

        hgl::strcpy(ua->name,SHADER_RESOURCE_NAME_MAX_LENGTH,sp,str-sp);

        return(true);
    }

    struct VertexInputBlockParse:public TextParse
    {
        List<UniformAttrib> input_list;

    public:

        bool OnLine(const char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            UniformAttrib ua;

            if(ParseUniformAttrib(&ua,text))
                input_list.Add(ua);

            return(true);
        }
    };//struct VertexInputBlockParse

    struct ShaderBlockParse:public TextParse
    {
        bool                    output=false; 
        List<UniformAttrib>     output_list;

        bool                    code=false;
        CodeParse               code_parse;

    public:

        bool OnLine(const char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);
    
            if(code)
            {
                if(code_parse.OnLine(text,len))
                    code=false;

                return(true);  
            }

            if(output)
            {
                if(*text=='}')
                {
                    output=false;
                    return(true);
                }

                UniformAttrib ua;

                if(ParseUniformAttrib(&ua,text))
                    output_list.Add(ua);
            }

            if(hgl::stricmp(text,"Code",4)==0)
            {
                code=true;
            }
            else
            if(hgl::stricmp(text,"Output",6)==0)
            {
                output=true;
            }

            return(true);
        }
    };//struct ShaderBlockParse

    struct GeometryShaderBlockParse:public ShaderBlockParse
    {
        Prim input_prim;
        Prim output_prim;
        uint max_vertices;

    public:

        bool OnLine(const char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            if(hgl::stricmp(text,"in ",3)==0)
            {
                text+=3;

                while(*text==' '||*text=='\t')++text;

                const char *sp=text;

                while(hgl::isalpha(*text)||*text=='_')++text;

                const Prim ip=ParsePrimName(sp,text-sp);

                if(!CheckGeometryShaderIn(ip))
                    return(false);

                input_prim=ip;
                return(true);
            }
            else
            if(hgl::stricmp(text,"out ",4)==0)
            {
                text+=4;

                while(*text==' '||*text=='\t')++text;

                const char *sp=text;

                while(hgl::isalpha(*text)||*text=='_')++text;

                const Prim op=ParsePrimName(sp,text-sp);

                if(!CheckGeometryShaderOut(op))
                    return(false);

                output_prim=op;

                while(*text!=',')++text;

                while(*text==' '||*text=='\t'||*text==',')++text;

                hgl::stou(text,max_vertices);

                if(max_vertices<=0)
                    return(false);

                return(true);
            }

            if(!ShaderBlockParse::OnLine(text,len))
                return(false);

            return(true);
        }
        
    };//struct GeometryShaderBlockParse

    struct MaterialFileParse:public TextParse
    {
        MaterialFileBlock state;

        TextParse *parse;

    public:

        MaterialFileParse()
        {
            state=MaterialFileBlock::None;
            parse=nullptr;
        }

        ~MaterialFileParse()
        {
            SAFE_CLEAR(parse)
        }

        bool OnLine(const char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            if(*text=='#')
            {
                SAFE_CLEAR(parse)

                state=GetMaterialFileState(text+1,len-1);

                switch(state)
                {
                    case MaterialFileBlock::Material:           parse=new MaterialFileBlockParse;break;
                    case MaterialFileBlock::MaterialInstance:   parse=new MaterialInstanceStateParse;break;
                    case MaterialFileBlock::VertexInput:        parse=new VertexInputBlockParse;break;
                    case MaterialFileBlock::Vertex:           
                    case MaterialFileBlock::Fragment:           parse=new ShaderBlockParse;break;
                    case MaterialFileBlock::Geometry:           parse=new GeometryShaderBlockParse;break;

                    default:                                    state=MaterialFileBlock::None;return(false);
                }

                return(true);
            }

            if(parse)
                parse->OnLine(text,len);

            return(true);
        }
    };
}//namespace MaterialFile

bool LoadMaterialFromFile(const AnsiString &mtl_filename)
{
    const OSString mtl_osfn=ToOSString(mtl_filename+".mtl");

    const OSString mtl_os_filename=filesystem::MergeFilename(OS_TEXT("ShaderLibrary"),mtl_osfn);

    if(!filesystem::FileExist(mtl_os_filename))
        return(false);

    io::OpenFileInputStream fis(mtl_os_filename);

    if(!fis)
        return(false);

    MaterialFileParse mtp;

    io::TextInputStream tis(fis,0);

    tis.SetParseCallback(&mtp);

    return tis.Run();
}

MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &name,const MaterialCreateConfig *cfg)
{
    return nullptr;
}

//MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &name,const Material3DCreateConfig *cfg)
//{
//}
STD_MTL_NAMESPACE_END
