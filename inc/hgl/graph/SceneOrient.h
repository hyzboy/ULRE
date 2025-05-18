#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/SceneMatrix.h>
namespace hgl::graph
{
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

        void SetLocalNormal(const Vector3f &nor) {scene_matrix.SetLocalNormal(nor);}                                        ///<设置本地法线
        void SetLocalMatrix (const Matrix4f &mat){scene_matrix.SetLocalMatrix(mat);}                                        ///<设置本地矩阵
        void SetParentMatrix(const Matrix4f &mat){scene_matrix.SetParentMatrix(mat);}                                       ///<设置上级到世界空间变换矩阵

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
}//namespace hgl::graph
