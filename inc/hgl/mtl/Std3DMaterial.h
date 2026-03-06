#pragma once

#include<hgl/mtl/StdMaterial.h>
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{

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

}//namespace hgl::graph::mtl
