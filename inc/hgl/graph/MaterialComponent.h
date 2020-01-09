#ifndef HGL_GRAPH_MATERIAL_COMPONENT_INCLUDE
#define HGL_GRAPH_MATERIAL_COMPONENT_INCLUDE

#include<hgl/type/DataType.h>

#define BEGIN_MATERIAL_NAMESPACE namespace hgl{namespace graph{namespace material{
#define END_MATERIAL_NAMESPACE }}}

BEGIN_MATERIAL_NAMESPACE
    enum class Component
    {
        ShadingModel=0,

        Color,
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
    };//

    enum class ComponentBit
    {
    #define MC_BIT_DEFINE(name) name=1<<(uint)Component::name
        MC_BIT_DEFINE(ShadingModel      ),

        MC_BIT_DEFINE(Color             ),
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
        Int,
        Uint,
    };//

    using ComponentBitsConfig=uint32;

    constexpr ComponentBitsConfig MCC_PureColor     =uint32(ComponentBit::Color);
    constexpr ComponentBitsConfig MCC_PureNormal    =uint32(ComponentBit::Normal);
    constexpr ComponentBitsConfig MCC_ColorNormal   =uint32(ComponentBit::Color)|uint32(ComponentBit::Normal);
    constexpr ComponentBitsConfig MCC_CNMR          =uint32(ComponentBit::Color)|uint32(ComponentBit::Normal)|uint32(ComponentBit::Metallic)|uint32(ComponentBit::Roughness);

    struct ComponentConfig
    {
        Component           comp;               ///<成份ID
        ComponentDataType   type;               ///<数据类型
        uint                channels;           ///<通道数
        bool                LinearColorspace;   ///<是要求线性颜色空间
    };

    const ComponentConfig *GetConfig(const enum class Component c);
END_MATERIAL_NAMESPACE
#endif//HGL_GRAPH_MATERIAL_COMPONENT_INCLUDE
