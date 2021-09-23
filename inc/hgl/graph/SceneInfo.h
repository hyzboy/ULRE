#ifndef HGL_GRAPH_SCENE_INFO_INCLUDE
#define HGL_GRAPH_SCENE_INFO_INCLUDE

#include<hgl/math/Matrix.h>
#include<hgl/CompOperator.h>
namespace hgl
{
    namespace graph
    {
        /** 
         * MVPæÿ’Û
         */
        struct MVPMatrix
        {
            Matrix4f l2w;                   ///< Local to World
            Matrix4f inverse_l2w;

            Matrix4f mvp;                   ///< projection * view * local_to_world
            Matrix4f inverse_mvp;

        public:

            void Set(const Matrix4f &w,const Matrix4f &vp)
            {
                l2w=w;
                inverse_l2w=inverse(l2w);

                mvp=vp*l2w;
                inverse_mvp=inverse(mvp);
            }

            CompOperatorMemcmp(const MVPMatrix &);
        };//struct MVPMatrix

        constexpr size_t MVPMatrixBytes=sizeof(MVPMatrix);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_INFO_INCLUDE
