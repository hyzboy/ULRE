#ifndef HGL_GRAPH_SCENE_INFO_INCLUDE
#define HGL_GRAPH_SCENE_INFO_INCLUDE

#include<hgl/math/Matrix.h>
#include<hgl/CompOperator.h>
namespace hgl
{
    namespace graph
    {
        /** 
         * MVP矩阵
         */
        struct MVPMatrix
        {
            Matrix4f l2w;                   ///< Local to World
                                            //Matrix4f normal;                ///<transpose(inverse(mat3(l2w)));
            Matrix3x4f normal;              ///<这里用3x4，在Shader中是3x3(但实际它是3x4保存)

            Matrix4f mv;                    ///< view * local_to_world
            Matrix4f mvp;                   ///< projection * view * local_to_world

        public:

            void Set(const Matrix4f &w,const Matrix4f &vp,const Matrix4f &v)
            {
                l2w=w;
                normal=transpose(inverse(l2w));
                mv=v*l2w;
                mvp=vp*l2w;
            }

            CompOperatorMemcmp(const MVPMatrix &);
        };//struct MVPMatrix

        constexpr size_t MVPMatrixBytes=sizeof(MVPMatrix);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_INFO_INCLUDE
