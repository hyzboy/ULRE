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
            struct Material3DCreateConfig;

            class Std3DMaterial
            {
            protected:

                const Material3DCreateConfig *cfg;

                MaterialCreateInfo *mci;

            protected:

                virtual bool BeginCustomShader(){return true;/*some work before creating shader*/};

                virtual bool CustomVertexShader(ShaderCreateInfoVertex *);
                virtual bool CustomGeometryShader(ShaderCreateInfoGeometry *){return false;}
                virtual bool CustomFragmentShader(ShaderCreateInfoFragment *)=0;

                virtual bool EndCustomShader(){return true;/*some work after creating shader*/};

            public:

                Std3DMaterial(const Material3DCreateConfig *);
                virtual ~Std3DMaterial()=default;
    
                virtual MaterialCreateInfo *Create();
            };//class Std3DMaterial
        }//namespace mtl
    }//namespace graph
}//namespace hgl
