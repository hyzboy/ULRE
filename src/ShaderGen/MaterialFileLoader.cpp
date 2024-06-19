#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKShaderStage.h>
#include<hgl/graph/VKSamplerType.h>

#include<hgl/io/TextInputStream.h>
#include<hgl/filesystem/FileSystem.h>

#include<hgl/type/AccumMemoryManager.h>
#include<hgl/io/FileAccess.h>

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

    const uint32_t ShaderStageParse(char *text,const char *ep)
    {
        uint32_t shader_stage_flag_bits=0;
        char *sp;

        while(*text==' '||*text=='\t')++text;

        while(text<ep)
        {
            sp=text;

            while(hgl::isalpha(*text))
                ++text;

            shader_stage_flag_bits|=GetShaderStageFlagBits(sp,text-sp);

            ++text;
        }

        return shader_stage_flag_bits;
    }

    int ClipCodeString(char *str,const int max_len,const char *text)
    {
        while(*text==' '||*text=='\t')++text;

        const char *sp=text;

        while(hgl::iscodechar(*text))++text;

        hgl::strcpy(str,max_len,sp,text-sp);

        return text-sp;
    }

    int ClipFilename(char *str,const int max_len,const char *text)
    {
        while(*text==' '||*text=='\t')++text;

        const char *sp=text;

        while(hgl::isfilenamechar(*text))++text;

        hgl::strcpy(str,max_len,sp,text-sp);

        return text-sp;
    }

    struct UBOParse:public TextParse
    {
        UBOData ubo_data;

    public:

        UBOParse()=default;

        void Clear()
        {
            hgl_zero(ubo_data);
        }

        bool OnLine(char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            if(*text=='{')
            {
                ++text;
                while(*text=='\r'||*text=='\n')++text;

                return(false);
            }
            
            if(*text=='}')
            {
                *text=0;
                return(true);
            }

            while(*text==' '||*text=='\t')++text;
            if(text[0]=='/'&&text[1]=='/')
                return(false);

            if(hgl::stricmp(text,"File ",5)==0)
            {
                ClipFilename(ubo_data.filename,sizeof(ubo_data.filename),text+5);
            }
            else
            if(hgl::stricmp(text,"Struct ",7)==0)
            {
                ClipCodeString(ubo_data.struct_name,sizeof(ubo_data.struct_name),text+7);
            }
            else
            if(hgl::stricmp(text,"Name ",5)==0)
            {
                ClipCodeString(ubo_data.name,sizeof(ubo_data.name),text+5);
            }
            else
            if(hgl::stricmp(text,"Stage ",6)==0)
            {
                ubo_data.shader_stage_flag_bits=ShaderStageParse(text+6,text+len);
            }
            else
            if(hgl::stricmp(text,"Set ",4)==0)
            {
                char set_name[32];

                ClipCodeString(set_name,sizeof(set_name),text+4);

                ubo_data.set=GetDescriptorSetType(set_name);
            }

            return(false);
        }
    };//struct UBOParse

    struct MaterialBlockParse:public TextParse
    {
        MaterialFileBlock state;

        AnsiStringList *require_list=nullptr;

        UBODataList *ubo_list=nullptr;

        bool ubo=false;
        UBOParse ubo_parse;

    public:

        MaterialBlockParse(AnsiStringList *asl,UBODataList *udl)
        {
            state=MaterialFileBlock::None;

            require_list=asl;
            ubo_list=udl;
        }

        bool OnLine(char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            char *ep=text+len;

            if(ubo)
            {
                if(ubo_parse.OnLine(text,len))
                {
                    ubo_list->Add(ubo_parse.ubo_data);

                    ubo_parse.Clear();
                    ubo=false;
                }

                return(true);
            }

            if(hgl::stricmp(text,"Require ",8)==0)
            {
                text+=8;

                char *sp=text;

                while(sp<ep)
                {
                    while(hgl::iscodechar(*text))++text;

                    require_list->Add(AnsiString(sp,text-sp));

                    while(!hgl::iscodechar(*text))++text;

                    sp=text;
                }
            }
            else
            if(hgl::stricmp(text,"UBO",3)==0)
            {
                ubo=true;
            }

            return(true);
        }
    };//struct MaterialBlockParse

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

    struct MaterialInstanceBlockParse:public TextParse
    {
        bool        code                    =false;
        CodeParse   code_parse;

        uint        mi_bytes                =0;
        uint32_t    shader_stage_flag_bits  =0;

        MaterialInstanceData *mid=nullptr;

    public:

        MaterialInstanceBlockParse(MaterialInstanceData *d)
        {
            mid=d;
        }

        ~MaterialInstanceBlockParse()
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
            if(hgl::stricmp(text,"Length ",7)==0)
            {
                text+=7;
                while(*text==' '||*text=='\t')++text;

                hgl::stou(text,mi_bytes);
            }
            else
            if(hgl::stricmp(text,"Stage ",6)==0)
            {
                shader_stage_flag_bits=ShaderStageParse(text+6,text+len);
            }

            return(true);
        }
    };//struct MaterialInstanceBlockParse

    bool ParseInterpolation(Interpolation *inter,const char *name)
    {
        if(!inter)return(false);
        if(!name||!*name)return(false);

        for(int i=(int)Interpolation::Smooth;i<(int)Interpolation::Flat;i++)
            if(!strcmp(name,InterpolationName[i],sizeof(InterpolationName[i])))
            {
                *inter=Interpolation(i);
                return(true);
            }

        return(false);
    }

    bool ParseOutputItem(VIA *via,const char *str)
    {
        const char *sp;

        while(*str==' '||*str=='\t')++str;

        if(ParseInterpolation(&(via->interpolation),str))
        {
            while(*str!=' '&&*str!='\t')++str;
            while(*str==' '||*str=='\t')++str;
        }
        else
        {
            via->interpolation=Interpolation::Smooth;
        }

        VAType vat;

        if(!ParseVertexAttribType(&(vat),str))
            return(false);

        via->basetype=uint8(vat.basetype);
        via->vec_size=vat.vec_size;

        while(*str!=' '&&*str!='\t')++str;
        while(*str==' '||*str=='\t')++str;

        sp=str;

        while(hgl::iscodechar(*str))++str;

        hgl::strcpy(via->name,SHADER_RESOURCE_NAME_MAX_LENGTH,sp,str-sp);

        return(true);
    }

    bool ParseOutputItem(ShaderVariable *sv,const char *str)
    {
        const char *sp;

        while(*str==' '||*str=='\t')++str;

        if(ParseInterpolation(&(sv->interpolation),str))
        {
            while(*str!=' '&&*str!='\t')++str;
            while(*str==' '||*str=='\t')++str;
        }
        else
        {
            sv->interpolation=Interpolation::Smooth;
        }

        if(!sv->type.ParseTypeString(str))
            return(false);

        while(*str!=' '&&*str!='\t')++str;
        while(*str==' '||*str=='\t')++str;

        sp=str;

        while(hgl::iscodechar(*str))++str;

        hgl::strcpy(sv->name,SHADER_RESOURCE_NAME_MAX_LENGTH,sp,str-sp);

        return(true);
    }

    struct VertexInputBlockParse:public TextParse
    {
        List<VIA> *via_list=nullptr;

    public:

        VertexInputBlockParse(List<VIA> *ual)
        {
            via_list=ual;
        }

        bool OnLine(char *text,const int len) override
        {
            if(!text||!*text||len<=0)
                return(false);

            VIA via;

            if(ParseOutputItem(&via,text))
                via_list->Add(via);

            return(true);
        }
    };//struct VertexInputBlockParse

    template<typename SD,typename OIT>
    struct ShaderBlockParse:public TextParse
    {
        SD *                    shader_data=nullptr;

        bool                    output=false; 

        bool                    code=false;
        CodeParse               code_parse;

    public:

        ShaderBlockParse(SD *sd)
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

                OIT output_item;

                if(ParseOutputItem(&output_item,text))
                    shader_data->AddOutput(output_item);
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

    using VertexShaderBlockParse=ShaderBlockParse<VertexShaderData,ShaderVariable>;
    using FragmentShaderBlockParse=ShaderBlockParse<FragmentShaderData,VIA>;

    struct GeometryShaderBlockParse:public ShaderBlockParse<GeometryShaderData,ShaderVariable>
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
                    parse=new MaterialBlockParse(&(mfd->require_list),&(mfd->ubo_list));
                else
                if(state==MaterialFileBlock::MaterialInstance)
                    parse=new MaterialInstanceBlockParse(&(mfd->mi));
                else
                if(state==MaterialFileBlock::VertexInput)
                    parse=new VertexInputBlockParse(&(mfd->via));
                else
                if(state>=MaterialFileBlock::Vertex
                 &&state<=MaterialFileBlock::Fragment)
                {
                    ShaderData *sd=nullptr;

                    if(state==MaterialFileBlock::Vertex)
                    {
                        sd=new VertexShaderData();

                        parse=new VertexShaderBlockParse((VertexShaderData *)sd);
                    }
                    else
                    if(state==MaterialFileBlock::Geometry)
                    {
                        sd=new GeometryShaderData();

                        parse=new GeometryShaderBlockParse((GeometryShaderData *)sd);
                    }
                    else
                    if(state==MaterialFileBlock::Fragment)
                    {
                        sd=new FragmentShaderData();

                        parse=new FragmentShaderBlockParse((FragmentShaderData *)sd);
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

namespace
{
    constexpr const os_char HGL_SHADER_LIBRARY_FOLDER[]=OS_TEXT("ShaderLibrary");

    AccumMemoryManager ubo_memory;
    hgl::Map<OSString,AccumMemoryManager::Block *> ubo_codes_map;

    AccumMemoryManager::Block *LoadUBO2Block(const OSString &filename)
    {
        AccumMemoryManager::Block *block;

        if(ubo_codes_map.Get(filename,block))
            return block;

        hgl::io::FileAccess fa;

        if(!fa.OpenRead(filename))
        {
            ubo_codes_map.Add(filename,nullptr);
            return(nullptr);
        }

        const int64 size=fa.GetSize();

        block=ubo_memory.Acquire(size+1);

        char *ptr=(char *)ubo_memory.Access(block);

        fa.Read(ptr,size);

        ptr[size]=0;
        
        ptr+=size-1;
        while(*ptr=='\r'||*ptr=='\n')
        {
            *ptr=0;
            --ptr;
        }

        ubo_codes_map.Add(filename,block);

        return block;
    }
}//namespace

const char *GetUBOCodes(const AccumMemoryManager::Block *block)
{
    if(!block)return nullptr;

    return (const char *)ubo_memory.Access(block);
}

MaterialFileData *LoadMaterialDataFromFile(const AnsiString &mtl_filename)
{
    const OSString mtl_osfn=filesystem::FixFilename(ToOSString(mtl_filename+".mtl"));

    const OSString mtl_os_filename=filesystem::MergeFilename(HGL_SHADER_LIBRARY_FOLDER,mtl_osfn);

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

    if(mfd->ubo_list.GetCount()<=0)
        return mfd;
    
    const OSString mtl_path=filesystem::ClipPathname(mtl_os_filename,false);

    OSString ubo_os_fn;
    OSString ubo_os_full_filename;

    for(UBOData &ud:mfd->ubo_list)
    {
        ubo_os_fn=filesystem::FixFilename(ToOSString(ud.filename));

        if(ubo_os_fn.GetFirstChar()==HGL_DIRECTORY_SEPARATOR)
            ubo_os_full_filename=filesystem::MergeFilename(HGL_SHADER_LIBRARY_FOLDER,ubo_os_fn);
        else
            ubo_os_full_filename=filesystem::MergeFilename(mtl_path,ubo_os_fn);

        ud.block=LoadUBO2Block(ubo_os_full_filename);

        if(!ud.block)
        {
            delete mfd;
            return nullptr;
        }
    }

    return mfd;
}
STD_MTL_NAMESPACE_END
