#ifndef HGL_GRAPH_MATERIAL_PBR_INCLUDE
#define HGL_GRAPH_MATERIAL_PBR_INCLUDE

#include<hgl/graph/material/Material.h>
MATERIAL_NAMESPACE_BEGIN
/**
 * 标准PBR材质<br>
 * BaseColor/Normal/Metallic/Roughness四个属性必须都有。如未提供，则会使用const方式提供一个数值
 */
class PBRMaterial:public Material
{
public:

    PBRMaterial(const UTF8String &  n,
                const BlendMode &   bm  =BlendMode::Opaque,
                const bool          ts  =false,
                const bool          wf  =false):
                Material(n,MCC_CNMR,bm,ts,wf){}

    virtual ~PBRMaterial()=default;
};//class PBRMaterial:public Material
MATERIAL_NAMESPACE_END
#endif//HGL_GRAPH_MATERIAL_PBR_INCLUDE
