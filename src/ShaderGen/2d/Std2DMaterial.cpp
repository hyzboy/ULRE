#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/2d/Material2DConfig.h>

STD_MTL_NAMESPACE_BEGIN
Std2DMaterial::Std2DMaterial(const Material2DConfig *c)
{            
    MaterialCreateInfo *mci=new MaterialCreateInfo(c);
}

Std2DMaterial::~Std2DMaterial()
{
    delete mci;
}

bool Std2DMaterial::CreateVertexShader(ShaderCreateInfoVertex *vs)
{
}

bool Std2DMaterial::CreateFragmentShader(ShaderCreateInfoFragment *fs)
{
}
    
bool Std2DMaterial::Create()
{
}
STD_MTL_NAMESPACE_END
