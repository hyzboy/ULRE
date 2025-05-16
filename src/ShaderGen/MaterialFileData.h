#pragma once
#include<hgl/type/Map.h>
#include<hgl/type/StringList.h>
#include<hgl/type/AccumMemoryManager.h>
#include<hgl/graph/VKShaderStage.h>
#include<hgl/graph/VKSamplerType.h>
#include<hgl/graph/VKPrimitiveType.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/graph/mtl/ShaderVariableType.h>

namespace material_file
{
    using namespace hgl;
    using namespace hgl::graph;

    constexpr size_t SHADER_RESOURCE_NAME_MAX_LENGTH=VERTEX_ATTRIB_NAME_MAX_LENGTH;

    using ShaderNameVariable=char[SHADER_RESOURCE_NAME_MAX_LENGTH];

    struct MaterialInstanceData
    {
        const char *code;

        uint        code_length;

        uint        data_bytes;

        uint32_t    shader_stage_flag_bits;
    };

    struct SamplerData
    {
        ShaderNameVariable name;

        SamplerType type;
    };

    struct UBOData
    {
        ShaderNameVariable struct_name;
        ShaderNameVariable name;

        char filename[HGL_MAX_PATH];

        uint32_t shader_stage_flag_bits;

        hgl::graph::DescriptorSetType set;

        AccumMemoryManager::Block *block;
    };

    using UBODataList=ArrayList<UBOData>;

    struct ShaderData
    {
        VkShaderStageFlagBits   shader_stage;

        const char *            code;
        uint                    code_length;

        ArrayList<SamplerData>  sampler;

    public:

        ShaderData(VkShaderStageFlagBits ss)
        {
            shader_stage=ss;

            code=nullptr;
            code_length=0;
        }

        virtual ~ShaderData()=default;

        const VkShaderStageFlagBits GetShaderStage()const{return shader_stage;}
    };

    struct VertexShaderData:public ShaderData
    {
        SVList output;

        VertexShaderData():ShaderData(VK_SHADER_STAGE_VERTEX_BIT){}

    public:

        void AddOutput(const ShaderVariable &sv){output.Add(sv);}
    };

    struct GeometryShaderData:public ShaderData
    {
        PrimitiveType input_prim;
        PrimitiveType output_prim;
        uint max_vertices=0;

        SVList output;

    public:

        GeometryShaderData():ShaderData(VK_SHADER_STAGE_GEOMETRY_BIT){}

    public:

        void AddOutput(const ShaderVariable &sv){output.Add(sv);}
    };

    struct FragmentShaderData:public ShaderData
    {
        VIAList output;

        FragmentShaderData():ShaderData(VK_SHADER_STAGE_FRAGMENT_BIT){}

        void AddOutput(const VIA &via){output.Add(via);}
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

        MaterialInstanceData    mi_data{};

        VIAList                 via_list;                   ///<Vertex Input

        UBODataList             ubo_list;

        ShaderDataMap           shader_data_map;

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
