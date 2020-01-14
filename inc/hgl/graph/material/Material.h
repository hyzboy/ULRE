#ifndef HGL_GRAPH_MATERIAL_INCLUDE
#define HGL_GRAPH_MATERIAL_INCLUDE

#include<hgl/graph/material/Component.h>
#include<hgl/type/BaseString.h>

MATERIAL_NAMESPACE_BEGIN
    enum class BlendMode
    {
        Opaque=0,
        Mask,
        Alpha,
        Additive,
        Subtractive,
        Modulate,
        PreMultiAlpha,  // 预计算好一半的Alpha

        BEGIN_RANGE =Opaque,
        END_RANGE   =PreMultiAlpha,
        RANGE_SIZE  =END_RANGE-BEGIN_RANGE+1
    };//enum class BlendMode

    class Material
    {
        UTF8String          name;
        
        ComponentBitsConfig comp_cfg;
        BlendMode           blend_mode;

        bool                two_sided=false;
        bool                wire_frame=false;

    public:

        Material(const UTF8String &n,
        const ComponentBitsConfig &cbf,
        const BlendMode &bm=BlendMode::Opaque,
        const bool ts=false,
        const bool wf=false)
        {
            name=n;
            comp_cfg=cbf;
            blend_mode=bm;
            two_sided=false;
            wire_frame=false;
        }

        virtual ~Material()=default;
    };//class Material
MATERIAL_NAMESPACE_END
#endif//HGL_GRAPH_MATERIAL_INCLUDE
