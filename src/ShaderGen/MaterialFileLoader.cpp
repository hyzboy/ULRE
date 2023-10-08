#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKShaderStage.h>

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
        MaterialInstanceCode,

        Vertex,
        VertexInput,
        VertexOutput,
        VertexCode,

        Geometry,
        GeometryInput,
        GeometryOutput,
        GeometryCode,

        Fragment,
        FragmentInput,
        FragmentOutput,
        FragmentCode,

        ENUM_CLASS_RANGE(None,FragmentCode)
    };//enum class State

    constexpr const char *StateNameList[]=
    {
        "Material",
        "MaterialInstance",
        "Vertex",
        "Geometry",
        "Fragment",
    };

    const MaterialFileState GetMaterialFileState(const char *str)
    {
        for(int i=0;i<int(MaterialFileState::END_RANGE);i++)
            if(hgl::stricmp(str,StateNameList[i])==0)
                return MaterialFileState(i);

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

    struct MaterialInstanceStateParse:public MaterialFileParse
    {
                bool    code        =false;
        const   char *  code_sp     =nullptr;
        const   char *  code_ep     =nullptr;

        uint mi_bytes=0;
        uint32_t shader_stage_flag_bits=0;

    public:

        bool OnLine(const char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            if(code)
            {
                if(*text=='{')
                {
                    code_sp=text+1;
                    return(true);
                }

                if(*text=='}')
                {
                    code_ep=text-1;
                    code=false;
                    return(true);
                }
            }

            if(hgl::stricmp(text,"Code")==0)
            {
                code=true;
            }
            else
            if(hgl::stricmp(text,"Length")==0)
            {
                text+=7;
                while(*text==' '||*text=='\t')++text;

                hgl::stou(text,mi_bytes);
            }
            else
            if(hgl::stricmp(text,"Stage")==0)
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
    };

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

                state=GetMaterialFileState(text+1);

                switch(state)
                {
                    case MaterialFileState::Material:         parse=new MaterialStateParse;break;
                    case MaterialFileState::MaterialInstance: parse=new MaterialInstanceStateParse;break;
                    case MaterialFileState::Vertex:           parse=new VertexStateParse;break;
                    case MaterialFileState::Geometry:         parse=new GeometryStateParse;break;
                    case MaterialFileState::Fragment:         parse=new FragmentStateParse;break;

                    default:                                  state=MaterialFileState::None;return(false);
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
