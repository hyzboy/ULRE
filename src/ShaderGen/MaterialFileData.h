#pragma once
#include<hgl/type/Map.h>
#include<hgl/graph/VKShaderStage.h>

namespace material_file
{
    using namespace hgl;
    using namespace hgl::graph;

    struct UniformAttrib
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

    struct ShaderData
    {
        VkShaderStageFlagBits    shader_stage;

        const char *code;
        uint        code_length;

        List<UniformAttrib> output;

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

    struct MaterialFileData
    {
    private:

        char *                  data=nullptr;
        int                     data_length=0;
    
    public:

        MaterialInstanceData    mi{};

        List<UniformAttrib>     vi;

        ObjectMap<VkShaderStageFlagBits,ShaderData> shader;

    public:

        MaterialFileData(char *d,int dl)
        {
            data=d;
            data_length=dl;
        }

        ~MaterialFileData()
        {
            delete[] data;
        }
    };//struct MaterialFileData

}//namespace material_file