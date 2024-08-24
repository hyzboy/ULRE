#ifndef HGL_GRAPH_SCENE_ORIENT_INCLUDE
#define HGL_GRAPH_SCENE_ORIENT_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/math/Transform.h>
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
            
            Matrix4f parent_matrix;
            bool parent_matrix_dirty;

            Matrix4f local_matrix;
            bool local_matrix_dirty;

            TransformManager transform_manager;
            uint32 transform_version;

            uint32 local_to_world_matrix_version;

            // LocalToWorld = ParentMatrix * LocalMatrix * TransformMatrix

            Matrix4f local_to_world_matrix;                                                                                    ///<本地到世界矩阵
            Matrix4f inverse_local_to_world_matrix;                                                                             ///<世界到本地矩阵
            Matrix4f inverse_transpose_local_to_world_matrix;                                                                    ///<世界到本地矩阵的转置矩阵

            virtual void SetWorldMatrix(const Matrix4f &);

        public:

            SceneOrient();
            SceneOrient(const SceneOrient &);
            SceneOrient(const Matrix4f &);
            virtual ~SceneOrient()=default;

            void SetLocalMatrix(const Matrix4f &);                                                                          ///<设置本地矩阵

        public: 
            const Matrix4f &    GetLocalMatrix                          ()const{return local_matrix;}

            TransformManager &  GetTransform                            ()     {return transform_manager;}                  ///<取得变换管理器

            const Matrix4f &    GetLocalToWorldMatrix                   ()const{return local_to_world_matrix;}
            const Matrix4f &    GetInverseLocalToWorldMatrix            ()const{return inverse_local_to_world_matrix;}
            const Matrix4f &    GetInverseTransposeLocalToWorldMatrix   ()const{return inverse_transpose_local_to_world_matrix;}

        public:

            virtual bool        RefreshMatrix       (const Matrix4f &);                                                     ///<刷新到世界空间变换
        };//class SceneOrient
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_ORIENT_INCLUDE
