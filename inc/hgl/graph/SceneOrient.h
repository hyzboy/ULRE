#ifndef HGL_GRAPH_SCENE_ORIENT_INCLUDE
#define HGL_GRAPH_SCENE_ORIENT_INCLUDE

//#include<hgl/type/List.h>
#include<hgl/math/Math.h>
#include<hgl/graph/VK.h>
namespace hgl
{
    namespace graph
    {
        /**
        * 方向定位数据基类
        */
        class SceneOrient                                                                                                   ///场景定位类
        {
        protected:

            TransformManager transform_manager;

            Matrix4f ParentMatrix;                                                                                          ///<上级矩阵

            Matrix4f LocalMatrix;                                                                                           ///<本地到上一级矩阵
            Matrix4f LocalToWorldMatrix;                                                                                    ///<本地到世界矩阵
            Matrix4f InverseLocalToWorldMatrix;                                                                             ///<世界到本地矩阵
            Matrix4f InverseTransposeLocalToWorldMatrix;                                                                    ///<世界到本地矩阵的转置矩阵

            virtual void SetWorldMatrix(const Matrix4f &);

        public:

            SceneOrient();
            SceneOrient(const SceneOrient &);
            SceneOrient(const Matrix4f &);
            virtual ~SceneOrient()=default;

        public: 

            TransformManager *  GetTransform        (){return &transform_manager;}                                          ///<取得变换管理器

            const Matrix4f &    GetLocalMatrix                          ()const{return LocalMatrix;}
            const Matrix4f &    GetLocalToWorldMatrix                   ()const{return LocalToWorldMatrix;}
            const Matrix4f &    GetInverseLocalToWorldMatrix            ()const{return InverseLocalToWorldMatrix;}
            const Matrix4f &    GetInverseTransposeLocalToWorldMatrix   ()const{return InverseTransposeLocalToWorldMatrix;}

        public:

            virtual bool        RefreshMatrix       (const Matrix4f &);                                                    ///<刷新到世界空间变换
        };//class SceneOrient
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_ORIENT_INCLUDE
