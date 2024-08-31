#pragma once

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
        * LocalToWorldMatrix=ParnetMatrix * LocalMatrix * TransformMatrix<br>
        */
        class SceneMatrix :public VersionData<Matrix4f>
        {
        protected:

            Matrix4f parent_matrix;
            Matrix4f local_matrix;
            bool local_is_identity;

            Vector3f local_normal;

            TransformManager transform_manager;
            Matrix4f transform_matrix;

        protected:

            Vector3f OriginWorldPosition;       //变换前世界坐标
            Vector3f FinalWorldPosition;        //变换后世界坐标

            Vector3f OriginWorldNormal;         //变换前世界法线
            Vector3f FinalWorldNormal;          //变换后世界法线

        protected:

            Matrix4f inverse_local_to_world_matrix;                                                                     ///<世界到本地矩阵
            Matrix4f inverse_transpose_local_to_world_matrix;                                                           ///<世界到本地矩阵的转置矩阵

            void MakeNewestData(Matrix4f &local_to_world_matrix) override;                                              ///<生成最新的数据(需要派生类重载)

        public:

            void Clear();

            const Matrix4f &GetLocalMatrix()const{return local_matrix;}                                                 ///<取得本地矩阵
            const Vector3f &GetLocalNormal()const{return local_normal;}                                                 ///<取得本地法线

            const Matrix4f &GetLocalToWorldMatrix(){return GetNewestVersionData();}                                     ///<取得本地到世界矩阵
            const Matrix4f &GetInverseLocalToWorldMatrix(){UpdateNewestData();return inverse_local_to_world_matrix;}    ///<取得世界到本地矩阵
            const Matrix4f &GetInverseTransposeLocalToWorldMatrix()                                                     ///<取得世界到本地矩阵的转置矩阵
            {
                UpdateNewestData();
                return inverse_transpose_local_to_world_matrix;
            }

            TransformManager &GetTransform(){return transform_manager;}                                                 ///<取得变换管理器

            const Vector3f &GetWorldPosition()const{return FinalWorldPosition;}                                         ///<取得世界坐标
            const Vector3f &GetWorldNormal()const { return FinalWorldNormal; }                                          ///<取得世界法线

        public:

            SceneMatrix():VersionData(Identity4f){Clear();}
            SceneMatrix(SceneMatrix &so);
            SceneMatrix(const Matrix4f &mat):VersionData(Identity4f)
            {
                SetLocalMatrix(mat);
                UpdateVersion();
            }

            void SetLocalNormal(const Vector3f &normal)
            {
                //if(IsNearlyEqual(local_normal,normal))
                if(!hgl_cmp(local_normal,normal))
                    return;

                local_normal=normal;
                UpdateVersion();
            }

            void SetLocalMatrix(const Matrix4f &mat)
            {
                //if (IsNearlyEqual(local_matrix,mat))
                if(!hgl_cmp(local_matrix,mat))
                    return;

                local_matrix=mat;
                local_is_identity=IsIdentityMatrix(mat);

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

            virtual void Update();
        };//class SceneMatrix
    }//namespace graph
}//namespace hgl
