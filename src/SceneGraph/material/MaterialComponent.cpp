#include<hgl/graph/material/Component.h>

MATERIAL_NAMESPACE_BEGIN
    namespace
    {
        constexpr ComponentConfig material_component_config_list[]=
        {
#define MCC_DEFINE(name,dt,c,lc)    {Component::name,ComponentDataType::dt,c,lc}

            MCC_DEFINE(ShadingModel,    Uint,   1,  false   ),

            MCC_DEFINE(BaseColor,       Float,  3,  true    ),

            MCC_DEFINE(Opacity,         Float,  1,  false   ),
            MCC_DEFINE(Normal,          Float,  3,  false   ),

            MCC_DEFINE(Metallic,        Float,  1,  false   ),
            MCC_DEFINE(Roughness,       Float,  1,  false   ),

            MCC_DEFINE(Emissive,        Float,  3,  true    ),
            MCC_DEFINE(Refraction,      Float,  1,  false   ),

            MCC_DEFINE(AO,              Float,  1,  false   ),
            MCC_DEFINE(SSS,             Float,  1,  false   ),
            MCC_DEFINE(Height,          Float,  1,  false   ),

            MCC_DEFINE(RIM,             Float,  1,  false   ),
            MCC_DEFINE(ClearCoat,       Float,  1,  false   ),
            MCC_DEFINE(Anisotropy,      Float,  1,  false   )

#undef MCC_DEFINE
        };
    };//namespace

    const ComponentConfig *GetConfig(const enum class Component c)
    {
        if(c<=Component::BEGIN_RANGE
         ||c>=Component::END_RANGE)return(nullptr);

        return material_component_config_list+(uint)c;
    }
MATERIAL_NAMESPACE_END
