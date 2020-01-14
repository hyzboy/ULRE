#ifndef HGL_GRAPH_MATERIAL_COMPONENT_INCLUDE
#define HGL_GRAPH_MATERIAL_COMPONENT_INCLUDE

#include<hgl/type/DataType.h>

#define MATERIAL_NAMESPACE_BEGIN    namespace hgl{namespace graph{namespace material{
#define MATERIAL_NAMESPACE_END      }}}

#define MATERIAL_NAMESPACE          hgl::graph::material
#define MATERIAL_NAMESPACE_USING    using namespace MATERIAL_NAMESPACE;

MATERIAL_NAMESPACE_BEGIN
    enum class Component
    {
        ShadingModel=0,

        BaseColor,
        Mask,
        Opacity,
        Normal,

        Metallic,
        Roughness,

        Emissive,
        Refraction,

        AO,
        SSS,
        Height,

        RIM,
        ClearCoat,
        Anisotropy,

        BEGIN_RANGE =ShadingModel,
        END_RANGE   =Anisotropy,
        RANGE_SIZE  =END_RANGE-BEGIN_RANGE+1,
    };//enum class Component

    enum class ComponentBit
    {
    #define MC_BIT_DEFINE(name) name=1<<(uint)Component::name
        MC_BIT_DEFINE(ShadingModel      ),

        MC_BIT_DEFINE(BaseColor         ),
        MC_BIT_DEFINE(Mask              ),
        MC_BIT_DEFINE(Opacity           ),
        MC_BIT_DEFINE(Normal            ),

        MC_BIT_DEFINE(Metallic          ),
        MC_BIT_DEFINE(Roughness         ),

        MC_BIT_DEFINE(Emissive          ),
        MC_BIT_DEFINE(Refraction        ),

        MC_BIT_DEFINE(AO                ),
        MC_BIT_DEFINE(SSS               ),
        MC_BIT_DEFINE(Height            ),

        MC_BIT_DEFINE(RIM               ),
        MC_BIT_DEFINE(ClearCoat         ),
        MC_BIT_DEFINE(Anisotropy        ),
    #undef MC_BIT_DEFINE
    };//enum class ComponentBit

    enum class ComponentDataType
    {
        Bool=0,
        Float,
        Double,
        Int,
        Uint,
    };//enum class ComponentDataType

    enum class DataFormat
    {
        NONE=0,

    #define MATERIAL_DATA_FORMAT_DEFINE(short_name,type)    type                    =((uint(ComponentDataType::type)<<4)|1), \
                                                            Vector##2##short_name   =((uint(ComponentDataType::type)<<4)|2), \
                                                            Vector##3##short_name   =((uint(ComponentDataType::type)<<4)|3), \
                                                            Vector##4##short_name   =((uint(ComponentDataType::type)<<4)|4)

        MATERIAL_DATA_FORMAT_DEFINE(b,Bool  ),
        MATERIAL_DATA_FORMAT_DEFINE(f,Float ),
        MATERIAL_DATA_FORMAT_DEFINE(d,Double),
        MATERIAL_DATA_FORMAT_DEFINE(i,Int   ),
        MATERIAL_DATA_FORMAT_DEFINE(u,Uint  ),

    #undef MATERIAL_DATA_FORMAT_DEFINE
    };//enum class DataFormat

    inline const ComponentDataType  GetFormatBaseType   (const enum class DataFormat &df){return ComponentDataType(uint(df)>>4);}
    inline const uint               GetFormatChannels   (const enum class DataFormat &df){return uint(df)&7;}

    using ComponentBitsConfig=uint32;

    constexpr ComponentBitsConfig MCC_PureColor     =uint32(ComponentBit::BaseColor);
    constexpr ComponentBitsConfig MCC_PureNormal    =uint32(ComponentBit::Normal);
    constexpr ComponentBitsConfig MCC_PureOpacity   =uint32(ComponentBit::Opacity);
    constexpr ComponentBitsConfig MCC_ColorNormal   =uint32(ComponentBit::BaseColor)|uint32(ComponentBit::Normal);
    constexpr ComponentBitsConfig MCC_CNMR          =uint32(ComponentBit::BaseColor)|uint32(ComponentBit::Normal)|uint32(ComponentBit::Metallic)|uint32(ComponentBit::Roughness);

    struct ComponentConfig
    {
        Component           comp;               ///<成份ID
        ComponentDataType   type;               ///<数据类型
        uint                channels;           ///<通道数
        bool                LinearColorspace;   ///<是要求线性颜色空间
    };//struct ComponentConfig

    const ComponentConfig *GetConfig(const enum class Component c);
MATERIAL_NAMESPACE_END
#endif//HGL_GRAPH_MATERIAL_COMPONENT_INCLUDE
