#pragma once

#define STD_MTL_NAMESPACE_BEGIN namespace hgl{namespace graph{namespace mtl{
#define STD_MTL_NAMESPACE_END   }}}

#define STD_MTL_NAMESPACE_USING using namespace hgl::graph::mtl;

#define STD_MTL_FUNC_NAMESPACE_BEGIN namespace hgl{namespace graph{namespace mtl{namespace func{
#define STD_MTL_FUNC_NAMESPACE_END   }}}}

#define STD_MTL_FUNC NAMESPACE_USING using namespace hgl::graph::mtl::func;

namespace hgl
{
    namespace graph
    {
        class ShaderCreateInfoVertex;
        class ShaderCreateInfoGeometry;
        class ShaderCreateInfoFragment;

        namespace mtl
        {
            namespace func
            {
            }//namespace func

            class MaterialCreateInfo;

            class StdMaterial
            {
            protected:

                virtual bool BeginCustomShader(){return true;/*some work before creating shader*/};

                virtual bool CustomVertexShader(ShaderCreateInfoVertex *)=0;
                virtual bool CustomGeometryShader(ShaderCreateInfoGeometry *){return false;}
                virtual bool CustomFragmentShader(ShaderCreateInfoFragment *)=0;

                virtual bool EndCustomShader(){return true;/*some work after creating shader*/};

            public:

                StdMaterial()=default;
                virtual ~StdMaterial()=default;
    
                virtual MaterialCreateInfo *Create()=0;
            };//class StdMaterial
        }//namespace mtl
    }//namespace graph
}//namespace hgl
