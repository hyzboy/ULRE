#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>

STD_MTL_NAMESPACE_BEGIN

class Std2DMaterial:public StdMaterial
{
protected:

    const Material2DCreateConfig *cfg;

protected:

    virtual bool CustomVertexShader(ShaderCreateInfoVertex *) override;

public:

    Std2DMaterial(const Material2DCreateConfig *c):StdMaterial(c){cfg=c;}
    virtual ~Std2DMaterial()=default;
};//class Std2DMaterial

STD_MTL_NAMESPACE_END
