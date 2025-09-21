#ifndef HGL_SHADER_CREATE_INFO_INCLUDE
#define HGL_SHADER_CREATE_INFO_INCLUDE

#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKInterpolation.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/graph/mtl/ShaderVariableType.h>
#include<hgl/type/StringList.h>
#include<hgl/log/Log.h>

namespace hgl{namespace graph
{
struct SPVData;

class MaterialDescriptorInfo;
class ShaderDescriptorInfo;

struct UBODescriptor;
struct TextureSamplerDescriptor;

class ShaderCreateInfo
{
    OBJECT_LOGGER

protected:

    ShaderStage shader_stage;                      ///<着色器阶段

    MaterialDescriptorInfo *mdi;

protected:

    AnsiStringList define_macro_list;
    AnsiStringList define_value_list;
    int define_macro_max_length;
    int define_value_max_length;

    AnsiString output_struct;

    AnsiString mi_codes;

    ArrayList<const char *> user_data_list;
    ArrayList<const char *> function_list;
    AnsiString main_function;

    AnsiString final_shader;

    SPVData *spv_data;

protected:

    virtual bool ProcHeader(){return(true);}

    virtual bool ProcDefine();
    virtual bool ProcLayout(){return(true);}

    virtual bool ProcInput(ShaderCreateInfo *);

    virtual bool IsEmptyOutput()const=0;
    virtual void GetOutputStrcutString(AnsiString &){}
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

    bool AddDefine(const AnsiString &m,const AnsiString &v);

    void AddStruct(const AnsiString &);
    bool AddUBO(DescriptorSetType type,const UBODescriptor *sd);
    bool AddTextureSampler(DescriptorSetType type,const TextureSamplerDescriptor *sd);

    void AddUserData(const char *str){user_data_list.Add(str);}
    void AddFunction(const char *str){function_list.Add(str);}

    void SetMaterialInstance(UBODescriptor *,const AnsiString &);
    virtual void AddMaterialInstanceOutput()=0;

    void SetMain(const AnsiString &str){main_function=str;}
    void SetMain(const char *str,const int len)
    {
        main_function.fromString(str,len);
    }

    const AnsiString &GetOutputStruct()const{return output_struct;}
    const AnsiString &GetShaderSource()const{return final_shader;}

    bool CreateShader(ShaderCreateInfo *);

    const uint32 *GetSPVData()const;
    const size_t GetSPVSize()const;
};//class ShaderCreateInfo
}}//namespace hgl::graph
#endif//HGL_SHADER_CREATE_INFO_INCLUDE
