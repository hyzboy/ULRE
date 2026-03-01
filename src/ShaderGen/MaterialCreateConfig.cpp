#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/vk/VertexAttrib.h>
#include<string>

namespace hgl::graph::mtl{

static const char *AsCStr(const AnsiString &text)
{
    return text.c_str()?text.c_str():"";
}

const AnsiString MaterialCreateConfig::ToHashString()
{
    std::string hash;
    hash.reserve(128);

    char str[16];
    char *p=str;

    *p='M';++p;

    if(material_instance){*p='I';++p;}

    *p='_';++p;

    *p='0'+rt_output.color;++p;

    if(rt_output.depth){*p='D';++p;}
    if(rt_output.stencil){*p='S';++p;}

    *p='_';++p;

    if(shader_stage_flag_bit&(uint32)ShaderStage::Vertex){*p='V';++p;}
    if(shader_stage_flag_bit&(uint32)ShaderStage::TessControl){*p='T';++p;}     //tc/te有一个就行了
    if(shader_stage_flag_bit&(uint32)ShaderStage::Geometry){*p='G';++p;}
    if(shader_stage_flag_bit&(uint32)ShaderStage::Fragment){*p='F';++p;}
    if(shader_stage_flag_bit&(uint32)ShaderStage::Compute){*p='C';++p;}
    if(shader_stage_flag_bit&(uint32)ShaderStage::Mesh){*p='M';++p;}     //mesh/task有一个就行了
    *p='_';++p;

    *p=0;

    hash=p;
    hash+=GetPrimName(prim);

    if(private_shader_buffer_source_count>0)
    {
        hash+="_PS";
        const std::string pss_count_str=std::to_string(private_shader_buffer_source_count);
        hash+=pss_count_str;

        for(uint32 i=0;i<private_shader_buffer_source_count;++i)
        {
            hash+="_";

            const ShaderBufferSource *sbs=private_shader_buffer_sources?private_shader_buffer_sources[i]:nullptr;
            if(sbs&&sbs->struct_name)
                hash+=sbs->struct_name;
            else
                hash+="null";
        }
    }

    return AnsiString(hash.c_str());
}

const AnsiString Material2DCreateConfig::ToHashString()
{
    std::string hash=AsCStr(MaterialCreateConfig::ToHashString());

    hash+=GetCoordinateSystem2DName(coordinate_system);

    if(local_to_world)
        hash+="_L2W";

    hash+="_";
    hash+=GetVertexAttribName(&position_format);

    return AnsiString(hash.c_str());
}

const AnsiString Material3DCreateConfig::ToHashString()
{
    std::string hash=AsCStr(MaterialCreateConfig::ToHashString());

    if(camera)
        hash+="_Camera";

    if(sky)
        hash+="_Sky";

    hash+="_Amb";
    char amb_model_str[2]={(char)('0'+(uint8)sky_ambient_model),0};
    hash+=amb_model_str;

    if(local_to_world)
        hash+="_L2W";

    hash+="_";
    hash+=GetVertexAttribName(&position_format);

    return AnsiString(hash.c_str());
}
}//namespace hgl::graph::mtl
