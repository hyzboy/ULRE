#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>

namespace hgl::graph::mtl{

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

}//namespace hgl::graph::mtl
