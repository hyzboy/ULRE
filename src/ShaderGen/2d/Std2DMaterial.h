#pragma once

#include<hgl/graph/mtl/StdMaterial.h>

namespace hgl
{
    namespace graph
    {
        class ShaderCreateInfoVertex;
        class ShaderCreateInfoFragment;

        namespace mtl
        {
            class MaterialCreateInfo;
            struct Material2DConfig;

            class Std2DMaterial
            {
            protected:

                Material2DConfig *cfg;

                MaterialCreateInfo *mci;

            protected:

                virtual bool CreateVertexShader(ShaderCreateInfoVertex *)=0;
                virtual bool CreateFragmentShader(ShaderCreateInfoFragment *)=0;

            public:

                Std2DMaterial(const Material2DConfig *);
                virtual ~Std2DMaterial();
    
                virtual bool Create()=0;
            };//class Std2DMaterial
        }//namespace mtl
    }//namespace graph
}//namespace hgl
