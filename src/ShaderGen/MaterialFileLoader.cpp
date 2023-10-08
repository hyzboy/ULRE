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
    using MaterialFileParse=io::TextInputStream::ParseCallback<char>;

    enum class MaterialFileState
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

    struct MaterialFileStateInfo
    {
        const char *name;
        const int len;
        MaterialFileState state;
    };

    #define MFS(name) {#name,sizeof(#name)-1,MaterialFileState::name},

    constexpr const MaterialFileStateInfo state_list[]=
    {
        MFS(Material)
        MFS(MaterialInstance)
        MFS(VertexInput)
        MFS(Vertex)
        MFS(Geometry)
        MFS(Fragment)
    };

    #undef MFS

    const MaterialFileState GetMaterialFileState(const char *str,const int len)
    {
        for(const MaterialFileStateInfo &info:state_list)
            if(len==info.len)
                if(hgl::stricmp(str,info.name,len)==0)
                    return info.state;

        return MaterialFileState::None;
    }

    struct MaterialStateParse:public MaterialFileParse
    {
        MaterialFileState state;

        MaterialStateParse()
        {
            state=MaterialFileState::None;
        }

        bool OnLine(const char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            return(true);    
        }
    };//struct MaterialStateParse

    struct CodeParse:public MaterialFileParse
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

    struct MaterialInstanceStateParse:public MaterialFileParse
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

    struct VertexInputStateParse:public MaterialFileParse
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
    };//struct VertexInputStateParse

    struct ShaderStateParse:public MaterialFileParse
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
    };//struct ShaderStateParse

    struct GeometryShaderStateParse:public ShaderStateParse
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

            if(!ShaderStateParse::OnLine(text,len))
                return(false);

            return(true);
        }
        
    };//struct GeometryShaderStateParse

    struct MaterialTextParse:public MaterialFileParse
    {
        MaterialFileState state;

        MaterialFileParse *parse;

    public:

        MaterialTextParse()
        {
            state=MaterialFileState::None;
            parse=nullptr;
        }

        ~MaterialTextParse()
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
                    case MaterialFileState::Material:           parse=new MaterialStateParse;break;
                    case MaterialFileState::MaterialInstance:   parse=new MaterialInstanceStateParse;break;
                    case MaterialFileState::VertexInput:        parse=new VertexInputStateParse;break;
                    case MaterialFileState::Vertex:           
                    case MaterialFileState::Fragment:           parse=new ShaderStateParse;break;
                    case MaterialFileState::Geometry:           parse=new GeometryShaderStateParse;break;

                    default:                                    state=MaterialFileState::None;return(false);
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

    MaterialTextParse mtp;

    io::TextInputStream tis(fis,0);

    tis.SetParseCallback(&mtp);

    return tis.Run();
}

MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &name,const MaterialCreateConfig *cfg)
{
    return nullptr;
}

//MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &name,const Material2DCreateConfig *cfg)
//{
//    Std2DMaterial *mtl=new Std2DMaterial(cfg);
//
//    
//}
//
//MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &name,const Material3DCreateConfig *cfg)
//{
//}
STD_MTL_NAMESPACE_END
