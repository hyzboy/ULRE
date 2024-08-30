#ifndef HGL_GRAPH_SCENE_ORIENT_INCLUDE
#define HGL_GRAPH_SCENE_ORIENT_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/math/Transform.h>
namespace hgl
{
    namespace graph
    {
        /**
        * 场景节点矩阵类<br>
        * 
        * 用于描述一个物体在3D空间中的位置、旋转、缩放等信息。<br>
        * 构成说明:<br>
        * <ul>
        *   <li>LocalMatrix 一般用于描述当前节点相对上一级的变换矩阵</li>
        *   <li>LocalToWorldMatrix 最终用于描述当前节点相对于世界的变换矩阵，在渲染时使用</li>
        * 
        *   <li>transform_manager 用于管理当前节点所有的变换情况，如果本节点不存在额外变换，数量为0。</li>
        * </ul>
        * 
        * LocalToWorldMatrix=ParnetMatrix * LocalMatrix * TraansformMatrix<br>
        */
        class SceneMatrix :public VersionData<Matrix4f>
        {
        protected:

            Matrix4f parent_matrix;
            Matrix4f local_matrix;
            TransformManager transform_manager;
            Matrix4f transform_matrix;

            Vector3f OriginWorldPosition;       //原始世界坐标
            Vector3f FinalWorldPosition;        //最终世界坐标

        protected:

            Matrix4f inverse_local_to_world_matrix;                                                                     ///<世界到本地矩阵
            Matrix4f inverse_transpose_local_to_world_matrix;                                                           ///<世界到本地矩阵的转置矩阵

            void MakeNewestData(Matrix4f &local_to_world_matrix) override                                               ///<生成最新的数据(需要派生类重载)
            {
                local_to_world_matrix=parent_matrix*local_matrix;

                OriginWorldPosition=TransformPosition(local_to_world_matrix,ZeroVector3f);

                if(transform_manager.IsEmpty())
                {
                    FinalWorldPosition=OriginWorldPosition;
                }
                else
                {
                    transform_manager.GetMatrix(transform_matrix,OriginWorldPosition);

                    local_to_world_matrix*=transform_matrix;

                    FinalWorldPosition=TransformPosition(local_to_world_matrix,ZeroVector3f);
                }
            
                inverse_local_to_world_matrix          =inverse(local_to_world_matrix);
                inverse_transpose_local_to_world_matrix=transpose(inverse_local_to_world_matrix);
            }

        public:

            void Clear()
            {
                parent_matrix=Identity4f;
                local_matrix=Identity4f;
                transform_matrix=Identity4f;
                UpdateVersion();
            }

            const Matrix4f &GetLocalMatrix()const{return local_matrix;}                                                 ///<取得本地矩阵

            const Matrix4f &GetLocalToWorldMatrix(){return GetNewestVersionData();}                                     ///<取得本地到世界矩阵
            const Matrix4f &GetInverseLocalToWorldMatrix(){UpdateNewestData();return inverse_local_to_world_matrix;}    ///<取得世界到本地矩阵
            const Matrix4f &GetInverseTransposeLocalToWorldMatrix()                                                     ///<取得世界到本地矩阵的转置矩阵
            {
                UpdateNewestData();
                return inverse_transpose_local_to_world_matrix;
            }

            TransformManager &GetTransform(){return transform_manager;}                                                 ///<取得变换管理器

            const Vector3f &GetWorldPosition()const{return FinalWorldPosition;}                                         ///<取得世界坐标

        public:

            SceneMatrix():VersionData(Identity4f)
            {
                Clear();
            }

            SceneMatrix(SceneMatrix &so):VersionData(so.GetLocalToWorldMatrix())
            {
                parent_matrix=so.parent_matrix;
                local_matrix=so.local_matrix;
                transform_manager=so.transform_manager;
                transform_matrix=so.transform_matrix;

                inverse_local_to_world_matrix=so.inverse_local_to_world_matrix;
                inverse_transpose_local_to_world_matrix=so.inverse_transpose_local_to_world_matrix;
                UpdateVersion();
            }

            SceneMatrix(const Matrix4f &mat):VersionData(Identity4f)
            {
                SetLocalMatrix(mat);
                UpdateVersion();
            }

            void SetLocalMatrix(const Matrix4f &mat)
            {
                //if (IsNearlyEqual(local_matrix,mat))
                if(!hgl_cmp(local_matrix,mat))
                    return;

                local_matrix=mat;
                UpdateVersion();
            }

            void SetParentMatrix(const Matrix4f &pm)
            {
                //if (IsNearlyEqual(parent_matrix,pm))
                if(!hgl_cmp(parent_matrix,pm))
                    return;

                parent_matrix=pm;
                UpdateVersion();
            }
        };//class SceneMatrix

        /**
        * 方向定位数据基类<br>
        */
        class SceneOrient                                                                                                   ///场景定位类
        {
        protected:

            SceneMatrix scene_matrix;

        public:

            SceneOrient()=default;
            SceneOrient(const SceneOrient &);
            SceneOrient(const Matrix4f &);
            virtual ~SceneOrient()=default;

            virtual void Clear()
            {
                scene_matrix.Clear();
            }

            void SetLocalMatrix  (const Matrix4f &mat){scene_matrix.SetLocalMatrix(mat);}               ///<设置本地矩阵
            void SetParentMatrix (const Matrix4f &mat){scene_matrix.SetParentMatrix(mat);}              ///<设置上级到世界空间变换矩阵

        public:

            const uint32        GetLocalToWorldMatrixVersion()const             {return scene_matrix.GetNewestVersion();}       ///<取得版本号

            const Vector3f &    GetWorldPosition() const                        {return scene_matrix.GetWorldPosition();}       ///<取得世界坐标
            const Matrix4f &    GetLocalMatrix                          ()const {return scene_matrix.GetLocalMatrix();}         ///<取得本地矩阵

            TransformManager &  GetTransform                            ()      {return scene_matrix.GetTransform();}           ///<取得变换管理器

            const Matrix4f &    GetLocalToWorldMatrix                   ()      {return scene_matrix.GetLocalToWorldMatrix();}  ///<取得本地到世界矩阵
            const Matrix4f &    GetInverseLocalToWorldMatrix            ()      {return scene_matrix.GetInverseLocalToWorldMatrix();}
            const Matrix4f &    GetInverseTransposeLocalToWorldMatrix   ()      {return scene_matrix.GetInverseTransposeLocalToWorldMatrix();}

        public:

            virtual void RefreshMatrix();
        };//class SceneOrient
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_ORIENT_INCLUDE
