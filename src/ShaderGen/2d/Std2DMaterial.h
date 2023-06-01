#pragma once

#include<hgl/graph/mtl/StdMaterial.h>

namespace hgl
{
    namespace graph
    {
        class ShaderCreateInfoVertex;
        class ShaderCreateInfoGeometry;
        class ShaderCreateInfoFragment;

        namespace mtl
        {
            class MaterialCreateInfo;
            struct Material2DConfig;

            class Std2DMaterial
            {
            protected:

                const Material2DConfig *cfg;

                MaterialCreateInfo *mci;

            protected:

                virtual bool BeforeCreateShader(){return true;/*some work before creating shader*/};

                virtual bool CreateVertexShader(ShaderCreateInfoVertex *);
                virtual bool CreateGeometryShader(ShaderCreateInfoGeometry *){return false;}
                virtual bool CreateFragmentShader(ShaderCreateInfoFragment *)=0;

                virtual bool AfterCreateShader(){return true;/*some work after creating shader*/};

            public:

                Std2DMaterial(const Material2DConfig *);
                virtual ~Std2DMaterial()=default;
    
                virtual MaterialCreateInfo *Create();
            };//class Std2DMaterial
        }//namespace mtl
    }//namespace graph
}//namespace hgl
