#ifndef HGL_GRAPH_MATERIAL_INCLUDE
#define HGL_GRAPH_MATERIAL_INCLUDE

#include<hgl/graph/MaterialComponent.h>
BEGIN_MATERIAL_NAMESPACE
    enum class BlendMode
    {
        Opaque=0,
        Mask,
        Alpha,
        Additive,
        Modulate,
        PreMultiAlpha,  // 预计算好一半的Alpha

        BEGIN_RANGE =Opaque,
        END_RANGE   =PreMultiAlpha,
        RANGE_SIZE  =END_RANGE-BEGIN_RANGE+1
    };//

    class Material
    {
        BlendMode           blend_mode;
        ComponentBitsConfig comp_cfg;
    };//class Material
END_MATERIAL_NAMESPACE
#endif//HGL_GRAPH_MATERIAL_INCLUDE
