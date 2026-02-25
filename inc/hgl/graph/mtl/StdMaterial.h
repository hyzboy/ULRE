#pragma once

#include<hgl/type/String.h>

namespace hgl::graph
{
    class ShaderCreateInfoVertex;
    class ShaderCreateInfoGeometry;
    class ShaderCreateInfoFragment;
    struct VulkanDevAttr;

    namespace mtl
    {
        enum class WithSky:uint8
        {
            Without=0,
            With
        };

        enum class WithCamera:uint8
        {
            Without=0,
            With
        };

        enum class WithLocalToWorld:uint8
        {
            Without=0,
            With
        };

        class MaterialCreateInfo;
        struct MaterialCreateConfig;

        class StdMaterial
        {
        protected:

            MaterialCreateInfo *mci;

        protected:

            virtual bool BeginCustomShader(){return true;/*some work before create shader*/};

            virtual bool CustomVertexShader(ShaderCreateInfoVertex *)=0;
            virtual bool CustomGeometryShader(ShaderCreateInfoGeometry *){return false;}
            virtual bool CustomFragmentShader(ShaderCreateInfoFragment *)=0;

            virtual bool EndCustomShader(){return true;/*some work after create shader*/};

        public:

            StdMaterial(const MaterialCreateConfig *);
            virtual ~StdMaterial()=default;

            virtual MaterialCreateInfo *Create(const VulkanDevAttr *dev_attr);

            /// Build GLSL source text for all stages without SPV compilation.
            /// dev_attr may be nullptr; ubo_range/ssbo_range default to desktop-safe values.
            virtual MaterialCreateInfo *CreateGLSLOnly(const VulkanDevAttr *dev_attr=nullptr);
        };//class StdMaterial
    }//namespace mtl
}//namespace hgl::graph
