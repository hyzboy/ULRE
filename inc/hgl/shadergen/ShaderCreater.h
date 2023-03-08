#ifndef HGL_SHADER_CREATER_INCLUDE
#define HGL_SHADER_CREATER_INCLUDE

#include<hgl/shadergen/ShaderDescriptorManager.h>

SHADERGEN_NAMESPACE_BEGIN
class ShaderCreater
{
    VkShaderStageFlagBits shader_stage;                      ///<着色器阶段

protected:

    AnsiString shader_codes;

public:

    ShaderDescriptorManager sdm;

    VkShaderStageFlagBits GetShaderStage()const{return shader_stage;}

public:

    ShaderCreater(VkShaderStageFlagBits ss):sdm(ss)
    {
        shader_stage=ss;
    }

    virtual ~ShaderCreater()=default;

    int AddOutput(const VAT &type,const AnsiString &name);
    int AddOutput(const AnsiString &type,const AnsiString &name);

    void SetShaderCodes(const AnsiString &str)
    {
        shader_codes=str;
    }

    bool CompileToSPV();
};//class ShaderCreater
SHADERGEN_NAMESPACE_END
#endif//HGL_SHADER_CREATER_INCLUDE
