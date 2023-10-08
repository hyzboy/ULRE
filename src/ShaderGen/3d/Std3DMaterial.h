#pragma once

#include<hgl/graph/mtl/StdMaterial.h>

STD_MTL_NAMESPACE_BEGIN

struct Material3DCreateConfig;

class Std3DMaterial:public StdMaterial
{
protected:

    const Material3DCreateConfig *cfg;

    MaterialCreateInfo *mci;

protected:

    virtual bool CustomVertexShader(ShaderCreateInfoVertex *) override;

public:

    Std3DMaterial(const Material3DCreateConfig *);
    virtual ~Std3DMaterial()=default;
    
    virtual MaterialCreateInfo *Create() override;
};//class Std3DMaterial

STD_MTL_NAMESPACE_END