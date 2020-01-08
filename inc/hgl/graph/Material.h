#ifndef HGL_GRAPH_MATERIAL_INCLUDE
#define HGL_GRAPH_MATERIAL_INCLUDE

#include<hgl/graph/MaterialData.h>
namespace hgl
{
    namespace graph
    {
        class Material
        {
        };//

        enum class MaterialBlendMode
        {
            Opaque=0,
            Mask,
            Alpha,
            Additive,
            Modulate,
            PreMultiAlpha,  //预计算好一半的Alpha

            BEGIN_RANGE =Opaque,
            END_RANGE   =PreMultiAlpha,
            RANGE_SIZE  =END_RANGE-BEGIN_RANGE+1
        };//

        enum class MaterialComponent
        {
            Color=0,
            Normal,
            Tangent,

            Metallic,
            Roughness,
            Emissive,
            Specular,

            Anisotropy,

            Opacity,

            SubsurfaceColor,
            AmbientOcclusion,
            Refraction,

            Rotation,
            IOR,

            ShadingModel,

            BEGIN_RANGE =Opaque,
            END_RANGE   =ShadingModel,
            RANGE_SIZE  =END_RANGE-BEGIN_RANGE+1,
        };//

        enum class MaterialComponentBit
        {
#define MC_BIT_DEFINE(name) name=1<<MaterialComponent::name
            MC_BIT_DEFINE(Color             ),
            MC_BIT_DEFINE(Normal            ),
            MC_BIT_DEFINE(Tangent           ),

            MC_BIT_DEFINE(Metallic          ),
            MC_BIT_DEFINE(Roughness         ),
            MC_BIT_DEFINE(Emissive          ),
            MC_BIT_DEFINE(Specular          ),

            MC_BIT_DEFINE(Anisotropy        ),

            MC_BIT_DEFINE(Opacity           ),

            MC_BIT_DEFINE(SubsurfaceColor   ),
            MC_BIT_DEFINE(AmbientOcclusion  ),
            MC_BIT_DEFINE(Refraction        ),

            MC_BIT_DEFINE(Rotation          ),
            MC_BIT_DEFINE(IOR               ),

            MC_BIT_DEFINE(ShadingModel      )
        };//enum class MaterialComponentBit

        using MCB=MaterialComponentBit;

        using MaterialComponentConfig=uint32;

        constexpr MaterialComponentConfig MC_PureColor      =MCB::Color;
        constexpr MaterialComponentConfig MC_ColorNormal    =MCB::Color|MCB::Normal;
        constexpr MaterialComponentConfig MC_PBR            =MCB::Color|MCB::Normal|MCB::Metallic|MCB::Roughness;

        class Material
        {
            MaterialBlendMode       blend_mode;
            MaterialComponentConfig component_config;
        };//class Material
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_MATERIAL_INCLUDE
