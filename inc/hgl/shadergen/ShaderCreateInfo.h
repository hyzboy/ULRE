#ifndef HGL_SHADER_CREATE_INFO_INCLUDE
#define HGL_SHADER_CREATE_INFO_INCLUDE

#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKInterpolation.h>
#include<hgl/type/StringList.h>

namespace hgl{namespace graph
{
struct SPVData;

class MaterialDescriptorInfo;
class ShaderDescriptorInfo;

struct UBODescriptor;

class ShaderCreateInfo
{
protected:

    VkShaderStageFlagBits shader_stage;                      ///<着色器阶段

    MaterialDescriptorInfo *mdi;

protected:

    AnsiStringList define_macro_list;
    AnsiStringList define_value_list;
    uint32_t define_macro_max_length;
    uint32_t define_value_max_length;

    AnsiString output_struct;

    AnsiString mi_codes;

    AnsiStringList function_list;
    AnsiString main_function;

    AnsiString final_shader;

    SPVData *spv_data;

protected:

    virtual bool ProcHeader(){return(true);}

    virtual bool ProcDefine();

    virtual bool ProcSubpassInput();
    virtual bool ProcInput(ShaderCreateInfo *);
    virtual bool ProcOutput();

    virtual bool ProcStruct();

    virtual bool ProcMI();

    virtual bool ProcUBO();
    virtual bool ProcSSBO();
    virtual bool ProcConst();
    virtual bool ProcSampler();

    bool CompileToSPV();

public:

    ShaderDescriptorInfo *sdm;

    VkShaderStageFlagBits GetShaderStage()const{return shader_stage;}

public:

    ShaderCreateInfo(VkShaderStageFlagBits ss,MaterialDescriptorInfo *m);
    virtual ~ShaderCreateInfo();

    bool AddDefine(const AnsiString &m,const AnsiString &v);

    int AddOutput(const graph::VAT &type,const AnsiString &name,Interpolation inter=Interpolation::Smooth);
    int AddOutput(const AnsiString &type,const AnsiString &name,Interpolation inter=Interpolation::Smooth);

    void AddFunction(const AnsiString &str){function_list.Add(str);}

    void SetMaterialInstance(UBODescriptor *,const AnsiString &);

    void SetMain(const AnsiString &str){main_function=str;}

    const AnsiString &GetOutputStruct()const{return output_struct;}
    const AnsiString &GetShaderSource()const{return final_shader;}

    bool CreateShader(ShaderCreateInfo *);

    const uint32 *GetSPVData()const;
    const size_t GetSPVSize()const;
};//class ShaderCreateInfo
}}//namespace hgl::graph
#endif//HGL_SHADER_CREATE_INFO_INCLUDE
