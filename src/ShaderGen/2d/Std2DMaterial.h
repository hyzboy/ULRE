#pragma once

#include<hgl/graph/mtl/StdMaterial.h>

namespace hgl
{
    namespace graph
    {
        struct GPUDeviceAttribute;

        class ShaderCreateInfoVertex;
        class ShaderCreateInfoGeometry;
        class ShaderCreateInfoFragment;

        namespace mtl
        {
            class MaterialCreateInfo;
            struct Material2DCreateConfig;

            class Std2DMaterial
            {
            protected:

                const Material2DCreateConfig *cfg;

                MaterialCreateInfo *mci;

            protected:

                virtual bool BeginCustomShader(){return true;/*some work before creating shader*/};

                virtual bool CustomVertexShader(ShaderCreateInfoVertex *);
                virtual bool CustomGeometryShader(ShaderCreateInfoGeometry *){return false;}
                virtual bool CustomFragmentShader(ShaderCreateInfoFragment *)=0;

                virtual bool EndCustomShader(){return true;/*some work after creating shader*/};

            public:

                Std2DMaterial(const Material2DCreateConfig *);
                virtual ~Std2DMaterial()=default;
    
                virtual MaterialCreateInfo *Create();
            };//class Std2DMaterial
        }//namespace mtl
    }//namespace graph
}//namespace hgl
