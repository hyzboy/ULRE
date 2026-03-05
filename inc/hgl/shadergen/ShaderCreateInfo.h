#pragma once

#include<hgl/vk/VertexAttrib.h>
#include<hgl/vk/VK.h>
#include<hgl/vk/VKInterpolation.h>
#include<hgl/vk/VKDescriptorSetType.h>
#include<hgl/log/Log.h>
#include <ankerl/unordered_dense.h>
#include<string>
#include<vector>

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

    ankerl::unordered_dense::set<std::string> define_macro_set;
    ankerl::unordered_dense::set<std::string> define_line_set;

    std::string output_struct;

    std::string mi_codes;

    std::vector<const char *> user_data_list;
    std::vector<const char *> function_list;
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

protected:

    void Init(ShaderDescriptorInfo *sdi,MaterialDescriptorInfo *m);

public:

    ShaderCreateInfo();
    virtual ~ShaderCreateInfo();

    bool AddDefine(const std::string &m,const std::string &v);
    bool AddDefine(const char *m,const char *v)
    {
        return AddDefine(std::string(m?m:""),std::string(v?v:""));
    }
    bool AddDefineFromStdString(const std::string &m,const std::string &v)
    {
        return AddDefine(m,v);
    }

    void AddStruct(const std::string &);
    void AddStruct(const char *name)
    {
        AddStruct(std::string(name?name:""));
    }
    bool AddUBO(DescriptorSetType type,const UBODescriptor *sd);
    bool AddSSBO(DescriptorSetType type,const SSBODescriptor *sd);
    bool AddTexture(DescriptorSetType type,const TextureDescriptor *sd);
    bool AddTextureSampler(DescriptorSetType type,const TextureSamplerDescriptor *sd);

    void AddUserData(const char *str){user_data_list.push_back(str);}
    void AddFunction(const char *str){function_list.push_back(str);}

    void SetMaterialInstance(UBODescriptor *,const std::string &);
    void SetMaterialInstance(SSBODescriptor *,const std::string &);
    void SetMaterialInstance(UBODescriptor *ubo,const char *mi)
    {
        SetMaterialInstance(ubo,std::string(mi?mi:""));
    }
    void SetMaterialInstance(SSBODescriptor *ssbo,const char *mi)
    {
        SetMaterialInstance(ssbo,std::string(mi?mi:""));
    }
    virtual void AddMaterialInstanceOutput()=0;

    void SetMain(const std::string &str){main_function=str;}
    void SetMain(const char *str){main_function=str?str:"";}
    void SetMainFromStdString(const std::string &str)
    {
        main_function=str;
    }
    void SetMain(const char *str,const int len)
    {
        main_function.assign(str?str:"",str?size_t(len):0);
    }

    const std::string &GetOutputStruct()const{return output_struct;}
    const std::string &GetShaderSource()const{return final_shader;}

    bool CreateShader(ShaderCreateInfo *);

    const uint32 *GetSPVData()const;
    const size_t GetSPVSize()const;
};//class ShaderCreateInfo
}}//namespace hgl::graph
