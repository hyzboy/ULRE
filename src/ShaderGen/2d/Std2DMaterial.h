#pragma once

#include<hgl/graph/mtl/StdMaterial.h>

STD_MTL_NAMESPACE_BEGIN

struct Material2DCreateConfig;

class Std2DMaterial:public StdMaterial
{
protected:

    const Material2DCreateConfig *cfg;

    MaterialCreateInfo *mci;

protected:

    virtual bool CustomVertexShader(ShaderCreateInfoVertex *) override;

public:

    Std2DMaterial(const Material2DCreateConfig *);
    virtual ~Std2DMaterial()=default;
    
    virtual MaterialCreateInfo *Create() override;
};//class Std2DMaterial

STD_MTL_NAMESPACE_END
