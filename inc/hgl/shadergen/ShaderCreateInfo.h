#ifndef HGL_SHADER_CREATE_INFO_INCLUDE
#define HGL_SHADER_CREATE_INFO_INCLUDE

#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VK.h>

namespace hgl{namespace graph
{
struct SPVData;

class MaterialDescriptorInfo;
class ShaderDescriptorInfo;

class ShaderCreateInfo
{
protected:

    VkShaderStageFlagBits shader_stage;                      ///<着色器阶段

    MaterialDescriptorInfo *mdi;

protected:

    AnsiString main_codes;

    AnsiString output_struct;

    AnsiString final_shader;

    SPVData *spv_data;

protected:

    virtual bool ProcHeader(){return(true);}
    virtual bool ProcSubpassInput();
    virtual bool ProcInput(ShaderCreateInfo *);
    virtual bool ProcOutput();

    virtual bool ProcStruct();

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

    int AddOutput(const graph::VAT &type,const AnsiString &name);
    int AddOutput(const AnsiString &type,const AnsiString &name);

    void SetShaderCodes(const AnsiString &str)
    {
        main_codes=str;
    }

    const AnsiString &GetOutputStruct()const{return output_struct;}
    const AnsiString &GetShaderSource()const{return final_shader;}

    bool CreateShader(ShaderCreateInfo *);

    const uint32 *GetSPVData()const;
    const size_t GetSPVSize()const;
};//class ShaderCreateInfo
}}//namespace hgl::graph
#endif//HGL_SHADER_CREATE_INFO_INCLUDE
