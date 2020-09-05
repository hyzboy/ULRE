#ifndef HGL_GRAPH_MATERIAL_STANDARD_INCLUDE
#define HGL_GRAPH_MATERIAL_STANDARD_INCLUDE

#include<hgl/graph/material/Material.h>
MATERIAL_NAMESPACE_BEGIN
/**
 * 传统标准材质
 */
class StandardMaterial:public Material
{
public:

    enum class DataSource
    {
        Const,
        Param,
        Texture,
        Position,
    };//

    struct
    {
        Component   comp;   ///<成份
        DataSource  source; ///<来源
        DataFormat  format; ///<数据格式
    };

public:

    StandardMaterial(   const UTF8String &          n,
                        const ComponentBitsConfig & cbf,
                        const BlendMode &           bm  =BlendMode::Opaque,
                        const bool                  ts  =false,
                        const bool                  wf  =false):
                        Material(n,cbf,bm,ts,wf){}

    virtual ~StandardMaterial()=default;
};//class StandardMaterial:public Material
MATERIAL_NAMESPACE_END
#endif//HGL_GRAPH_MATERIAL_STANDARD_INCLUDE
