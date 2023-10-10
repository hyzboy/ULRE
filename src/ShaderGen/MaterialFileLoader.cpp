#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKShaderStage.h>
#include<hgl/graph/VKSamplerType.h>

#include<hgl/io/TextInputStream.h>
#include<hgl/filesystem/FileSystem.h>

#include"MaterialFileData.h"

STD_MTL_NAMESPACE_BEGIN

namespace
{
    using namespace material_file;

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
        char *name;
        int len;
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

        bool OnLine(char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            return(true);    
        }
    };//struct MaterialFileBlockParse

    struct CodeParse:public TextParse
    {
        char *start   =nullptr;
        char *end     =nullptr;

    public:

        bool OnLine(char *text,const int len) override
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
                *text=0;
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

        MaterialInstanceData *mid=nullptr;

    public:

        MaterialInstanceStateParse(MaterialInstanceData *d)
        {
            mid=d;
        }

        ~MaterialInstanceStateParse()
        {
            if(code_parse.start&&code_parse.end)
            {
                mid->code                   =code_parse.start;
                mid->code_length            =code_parse.end-code_parse.start;
                mid->mi_bytes               =mi_bytes;
                mid->shader_stage_flag_bits =shader_stage_flag_bits;
            }
        }

        bool OnLine(char *text,const int len) override
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
        List<UniformAttrib> *vi_list=nullptr;

    public:

        VertexInputBlockParse(List<UniformAttrib> *ual)
        {
            vi_list=ual;
        }

        bool OnLine(char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            UniformAttrib ua;

            if(ParseUniformAttrib(&ua,text))
                vi_list->Add(ua);

            return(true);
        }
    };//struct VertexInputBlockParse

    struct ShaderBlockParse:public TextParse
    {
        ShaderData *            shader_data=nullptr;

        bool                    output=false; 

        bool                    code=false;
        CodeParse               code_parse;

    public:

        ShaderBlockParse(ShaderData *sd)
        {
            shader_data=sd;
        }

        bool OnLine(char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);
    
            if(code)
            {
                if(code_parse.OnLine(text,len))
                {
                    code=false;

                    shader_data->code=code_parse.start;
                    shader_data->code_length=code_parse.end-code_parse.start;
                }

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
                    shader_data->output.Add(ua);
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
            else
            if(hgl::stricmp(text,"sampler",7)==0)
            {
                const char *sp=text;

                while(*text!=' '&&*text!='\t')++text;

                SamplerType st=ParseSamplerType(sp,text-sp);

                RANGE_CHECK_RETURN_FALSE(st);

                while(!hgl::iscodechar(*text))++text;

                sp=text;

                while(hgl::iscodechar(*text))++text;

                SamplerData sd;

                sd.type=st;
                hgl::strcpy(sd.name,SHADER_RESOURCE_NAME_MAX_LENGTH,sp,text-sp);

                shader_data->sampler.Add(sd);
            }
            else
            {
            }

            return(true);
        }
    };//struct ShaderBlockParse

    struct GeometryShaderBlockParse:public ShaderBlockParse
    {
        GeometryShaderData *gsd=nullptr;

    public:

        GeometryShaderBlockParse(GeometryShaderData *sd):ShaderBlockParse(sd)
        {
            gsd=sd;
        }

        bool OnLine(char *text,const int len) override
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

                gsd->input_prim=ip;
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

                gsd->output_prim=op;

                while(*text!=',')++text;

                while(*text==' '||*text=='\t'||*text==',')++text;

                hgl::stou(text,gsd->max_vertices);

                if(gsd->max_vertices<=0)
                    return(false);

                return(true);
            }

            if(!ShaderBlockParse::OnLine(text,len))
                return(false);

            return(true);
        }
        
    };//struct GeometryShaderBlockParse

    struct MaterialTextParse:public TextParse
    {
        MaterialFileBlock state;

        TextParse *parse;

        MaterialFileData *mfd;

    public:

        MaterialTextParse(MaterialFileData *fd)
        {
            state=MaterialFileBlock::None;
            parse=nullptr;

            mfd=fd;
        }

        ~MaterialTextParse()
        {
            SAFE_CLEAR(parse)
        }

        bool OnLine(char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            if(*text=='#')
            {
                SAFE_CLEAR(parse)

                state=GetMaterialFileState(text+1,len-1);

                if(state==MaterialFileBlock::Material)
                    parse=new MaterialFileBlockParse;
                else
                if(state==MaterialFileBlock::MaterialInstance)
                    parse=new MaterialInstanceStateParse(&(mfd->mi));
                else
                if(state==MaterialFileBlock::VertexInput)
                    parse=new VertexInputBlockParse(&(mfd->vi));
                else
                if(state>=MaterialFileBlock::Vertex
                 &&state<=MaterialFileBlock::Fragment)
                {
                    ShaderData *sd=nullptr;

                    if(state==MaterialFileBlock::Vertex)
                    {
                        sd=new ShaderData(VK_SHADER_STAGE_VERTEX_BIT);

                        parse=new ShaderBlockParse(sd);
                    }
                    else
                    if(state==MaterialFileBlock::Geometry)
                    {
                        sd=new GeometryShaderData(VK_SHADER_STAGE_GEOMETRY_BIT);

                        parse=new GeometryShaderBlockParse((GeometryShaderData *)sd);
                    }
                    else
                    if(state==MaterialFileBlock::Fragment)
                    {
                        sd=new ShaderData(VK_SHADER_STAGE_FRAGMENT_BIT);

                        parse=new ShaderBlockParse(sd);
                    }

                    if(!sd)
                        return(false);

                    mfd->shader.Add(sd->GetShaderStage(),sd);

                    mfd->shader_stage_flag_bit|=sd->GetShaderStage();
                }
                else
                {
                    state=MaterialFileBlock::None;
                    return(false);
                }

                return(true);
            }

            if(parse)
                parse->OnLine(text,len);

            return(true);
        }
    };
}//namespace MaterialFile

MaterialFileData *LoadMaterialDataFromFile(const AnsiString &mtl_filename)
{
    const OSString mtl_osfn=ToOSString(mtl_filename+".mtl");

    const OSString mtl_os_filename=filesystem::MergeFilename(OS_TEXT("ShaderLibrary"),mtl_osfn);

    if(!filesystem::FileExist(mtl_os_filename))
        return nullptr;

    char *data; //未来二进制版本的材质文件，也是使用LoadFileToMemory加载。这里为了统一，所以文本也这么操作。

    int size=filesystem::LoadFileToMemory(mtl_os_filename,(void **)&data,true);

    MaterialFileData *mfd=new MaterialFileData(data,size);

    MaterialTextParse mtp(mfd);

    io::TextInputStream tis(data,size);

    tis.SetParseCallback(&mtp);

    if(!tis.Run())
        return nullptr;

    return mfd;
}
STD_MTL_NAMESPACE_END
