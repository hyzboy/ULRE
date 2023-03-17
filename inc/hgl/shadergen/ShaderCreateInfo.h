#ifndef HGL_SHADER_CREATE_INFO_INCLUDE
#define HGL_SHADER_CREATE_INFO_INCLUDE

#include<hgl/shadergen/ShaderGenNamespace.h>
#include<hgl/graph/VertexAttrib.h>
SHADERGEN_NAMESPACE_BEGIN

class MaterialDescriptorManager;
class ShaderDescriptorManager;

class ShaderCreateInfo
{
protected:

    VkShaderStageFlagBits shader_stage;                      ///<着色器阶段

    MaterialDescriptorManager *mdm;

protected:

    AnsiString main_codes;

    AnsiString output_struct;

    AnsiString final_shader;

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

    ShaderDescriptorManager *sdm;

    VkShaderStageFlagBits GetShaderStage()const{return shader_stage;}

public:

    ShaderCreateInfo(VkShaderStageFlagBits ss,MaterialDescriptorManager *m);
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
};//class ShaderCreateInfo
SHADERGEN_NAMESPACE_END
#endif//HGL_SHADER_CREATE_INFO_INCLUDE
