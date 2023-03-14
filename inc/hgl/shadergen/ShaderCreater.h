#ifndef HGL_SHADER_CREATER_INCLUDE
#define HGL_SHADER_CREATER_INCLUDE

#include<hgl/shadergen/ShaderDescriptorManager.h>

SHADERGEN_NAMESPACE_BEGIN
class ShaderCreater
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
    virtual bool ProcInput(ShaderCreater *);
    virtual bool ProcOutput();

    virtual bool ProcStruct();

    virtual bool ProcUBO();
    virtual bool ProcSSBO();
    virtual bool ProcConst();
    virtual bool ProcSampler();

public:

    ShaderDescriptorManager sdm;

    VkShaderStageFlagBits GetShaderStage()const{return shader_stage;}

public:

    ShaderCreater(VkShaderStageFlagBits ss,MaterialDescriptorManager *m):sdm(ss)
    {
        shader_stage=ss;
        mdm=m;
    }

    virtual ~ShaderCreater()=default;

    int AddOutput(const VAT &type,const AnsiString &name);
    int AddOutput(const AnsiString &type,const AnsiString &name);

    void SetShaderCodes(const AnsiString &str)
    {
        main_codes=str;
    }

    const AnsiString &GetOutputStruct()const{return output_struct;}

    bool CreateShader(ShaderCreater *);

    const AnsiString &GetShaderSource()const{return final_shader;}

    bool CompileToSPV();
};//class ShaderCreater
SHADERGEN_NAMESPACE_END
#endif//HGL_SHADER_CREATER_INCLUDE
