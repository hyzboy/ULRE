#include"Std2DMaterial.h"

STD_MTL_NAMESPACE_BEGIN
class Std2DMaterialLoader:public Std2DMaterial
{
public:

    using Std2DMaterial::Std2DMaterial;
    ~Std2DMaterialLoader()=default;

    bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override{return true;}
    bool CustomGeometryShader(ShaderCreateInfoGeometry *) override{return true;}
    bool CustomFragmentShader(ShaderCreateInfoFragment *) override{return true;}

    bool EndCustomShader() override{return true;}
};//class Std2DMaterialLoader:public Std2DMaterial

MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &name,const Material2DCreateConfig *cfg)
{
    Std2DMaterialLoader *mtl=new Std2DMaterialLoader(cfg);

    return nullptr;
}
STD_MTL_NAMESPACE_END
