#pragma once
#include<hgl/type/Map.h>
#include<hgl/type/StringList.h>
#include<hgl/graph/VKShaderStage.h>
#include<hgl/graph/VKSamplerType.h>
#include<hgl/graph/VKPrimitiveType.h>

namespace material_file
{
    using namespace hgl;
    using namespace hgl::graph;

    struct ShaderIOAttrib
    {
        VAT vat;
        
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];
    };

    struct MaterialInstanceData
    {
        const char *code;
        uint        code_length;

        uint        mi_bytes;

        uint32_t    shader_stage_flag_bits;
    };

    struct SamplerData
    {
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];

        SamplerType type;
    };

    struct UBOData
    {
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];

        char filename[HGL_MAX_PATH];

        uint32_t shader_stage_flag_bits;

        const char *codes;
        uint code_length;

        //为什么不使用AnsiString保存ubo shader codes ?
        //  1.MaterialFileData中使用UBODataList也就是List<UBOData>，会出现问题
        //  2.后台将所有UBO文件缓存，所以只传递过来一个char *即可
    };

    using UBODataList=List<UBOData>;

    struct ShaderData
    {
        VkShaderStageFlagBits   shader_stage;

        const char *            code;
        uint                    code_length;

        List<ShaderIOAttrib>    output;

        List<SamplerData>       sampler;

    public:

        ShaderData(VkShaderStageFlagBits ss)
        {
            shader_stage=ss;

            code=nullptr;
            code_length=0;
        }

        const VkShaderStageFlagBits GetShaderStage()const{return shader_stage;}
    };

    struct GeometryShaderData:public ShaderData
    {
        Prim input_prim;
        Prim output_prim;
        uint max_vertices=0;

    public:

        using ShaderData::ShaderData;        
    };

    using ShaderDataMap=ObjectMap<VkShaderStageFlagBits,ShaderData>;

    struct MaterialFileData
    {
    private:

        //不管是文本版还是二进制版
        //其中的代码段数据解析后都是放的指针，并无复制出来。
        //所以这里需要保存原始的文件数据

        char *                  data=nullptr;
        int                     data_length=0;
    
    public:

        AnsiStringList          require_list;               ///<需求的内部模块(如LocalToWorld,Sun,Shadow等系统内置元素)
//        AnsiStringList          import_list;                ///<引用的外部模块

        MaterialInstanceData    mi{};

        List<ShaderIOAttrib>    vi;                         ///<Vertex Input

        UBODataList             ubo_list;

        ShaderDataMap           shader;

        uint32_t                shader_stage_flag_bit;

    public:

        MaterialFileData(char *d,int dl)
        {
            data=d;
            data_length=dl;
            shader_stage_flag_bit=0;
        }

        ~MaterialFileData()
        {
            delete[] data;
        }
    };//struct MaterialFileData
}//namespace material_file
