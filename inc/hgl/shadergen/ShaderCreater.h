#ifndef HGL_SHADER_CREATER_INCLUDE
#define HGL_SHADER_CREATER_INCLUDE

#include<hgl/shadergen/ShaderDescriptorManager.h>

SHADERGEN_NAMESPACE_BEGIN
class ShaderCreater
{
    VkShaderStageFlagBits shader_stage;                      ///<着色器阶段

protected:

    AnsiString main_codes;

public:

    ShaderDescriptorManager sdm;

    VkShaderStageFlagBits GetShaderStage()const{return shader_stage;}

public:

    ShaderCreater(VkShaderStageFlagBits ss):sdm(ss)
    {
        shader_stage=ss;
    }

    virtual ~ShaderCreater()=default;

    int AddInput(const VAT &type,const AnsiString &name);
    int AddInput(const AnsiString &type,const AnsiString &name);

    int AddOutput(const VAT &type,const AnsiString &name);
    int AddOutput(const AnsiString &type,const AnsiString &name);

    bool AddFunction(const AnsiString &return_type,const AnsiString &func_name,const AnsiString &param_list,const AnsiString &codes);
};//class ShaderCreater
SHADERGEN_NAMESPACE_END
#endif//HGL_SHADER_CREATER_INCLUDE