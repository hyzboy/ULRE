#include <string>
#include <vector>
#pragma once

#include<hgl/vk/VertexAttrib.h>
#include<hgl/vk/VK.h>
#include<hgl/vk/VKInterpolation.h>
#include<hgl/vk/VKDescriptorSetType.h>
#include<hgl/graph/mtl/ShaderVariableType.h>
#include<hgl/log/Log.h>

namespace hgl{namespace graph
{
struct SPVData;

class MaterialDescriptorInfo;
class ShaderDescriptorInfo;

struct UBODescriptor;
struct SSBODescriptor;
struct TextureDescriptor;
struct TextureSamplerDescriptor;

class ShaderCreateInfo
{
    OBJECT_LOGGER

protected:

    ShaderStage shader_stage;                      ///<着色器阶段

    MaterialDescriptorInfo *mdi;

protected:

    std::vector<std::string> define_macro_list;
    std::vector<std::string> define_value_list;
    int define_macro_max_length;
    int define_value_max_length;

    std::string output_struct;

    std::string mi_codes;

    ValueArray<const char *> user_data_list;
    ValueArray<const char *> function_list;
    std::string main_function;

    std::string final_shader;

    SPVData *spv_data;

protected:

    virtual bool ProcHeader(){return(true);}

    virtual bool ProcDefine();
    virtual bool ProcLayout(){return(true);}

    virtual bool ProcInput(ShaderCreateInfo *);

    virtual bool IsEmptyOutput()const=0;
    virtual void GetOutputStrcutString(std::string &){}
    virtual bool ProcOutput();

    virtual bool ProcStruct();

    virtual bool ProcMI();

    virtual bool ProcUBO();
    virtual bool ProcSSBO();
    virtual bool ProcConstantID();
    virtual bool ProcSampler();

    bool CompileToSPV();

public:

    virtual ShaderDescriptorInfo *GetSDI()=0;
    const ShaderStage GetShaderStage()const{return shader_stage;}
    const VkShaderStageFlagBits GetVkShaderStage()const{return (VkShaderStageFlagBits)shader_stage;}

protected:

    void Init(ShaderDescriptorInfo *sdi,MaterialDescriptorInfo *m);

public:

    ShaderCreateInfo();
    virtual ~ShaderCreateInfo();

    bool AddDefine(const std::string &m,const std::string &v);

    void AddStruct(const std::string &);
    bool AddUBO(DescriptorSetType type,const UBODescriptor *sd);
    bool AddSSBO(DescriptorSetType type,const SSBODescriptor *sd);
    bool AddTexture(DescriptorSetType type,const TextureDescriptor *sd);
    bool AddTextureSampler(DescriptorSetType type,const TextureSamplerDescriptor *sd);

    void AddUserData(const char *str){user_data_list.Add(str);}
    void AddFunction(const char *str){function_list.Add(str);}

    void SetMaterialInstance(UBODescriptor *,const std::string &);
    void SetMaterialInstance(SSBODescriptor *,const std::string &);
    virtual void AddMaterialInstanceOutput()=0;

    void SetMain(const std::string &str){main_function=str;}
    void SetMain(const char *str,const int len)
    {
        main_function.fromString(str,len);
    }

    const std::string &GetOutputStruct()const{return output_struct;}
    const std::string &GetShaderSource()const{return final_shader;}

    bool CreateShader(ShaderCreateInfo *);

    const uint32 *GetSPVData()const;
    const size_t GetSPVSize()const;
};//class ShaderCreateInfo
}}//namespace hgl::graph
