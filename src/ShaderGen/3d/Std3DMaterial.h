#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>

STD_MTL_NAMESPACE_BEGIN

class Std3DMaterial:public StdMaterial
{
protected:

    const Material3DCreateConfig *cfg;

protected:

    virtual bool CustomVertexShader(ShaderCreateInfoVertex *) override;

public:

    Std3DMaterial(const Material3DCreateConfig *c):StdMaterial(c){cfg=c;}
    virtual ~Std3DMaterial()=default;
};//class Std3DMaterial

STD_MTL_NAMESPACE_END