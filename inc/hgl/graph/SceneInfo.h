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
            Matrix4f model;                 ///< model: Local to World
                                            //Matrix4f normal;                ///<transpose(inverse(mat3(model)));
            Matrix3x4f normal;              ///<这里用3x4，在Shader中是3x3(但实际它是3x4保存)

            Matrix4f mv;                    ///< view * model
            Matrix4f mvp;                   ///< projection * view * model

        public:

            void Set(const Matrix4f &local_to_world,const Matrix4f &view_projection,const Matrix4f &view)
            {
                model   =local_to_world;
                normal  =transpose(inverse(model));
                mv      =view*model;
                mvp     =view_projection*model;
            }

            CompOperatorMemcmp(const MVPMatrix &);
        };//struct MVPMatrix

        constexpr size_t MVPMatrixBytes=sizeof(MVPMatrix);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_INFO_INCLUDE
