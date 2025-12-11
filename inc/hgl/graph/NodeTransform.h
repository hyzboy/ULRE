#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/TransformState.h>
namespace hgl::graph
{
    /**
    * 方向定位数据基类<br>
    */
    class NodeTransform                                                                                                   ///场景定位类
    {
    protected:

        TransformState transform_state;

    public:

        NodeTransform()=default;
        NodeTransform(const NodeTransform &);
        NodeTransform(const Matrix4f &);
        virtual ~NodeTransform()=default;

        virtual void Reset()
        {
            transform_state.Reset();
        }

        void SetTransformState (const TransformState &sm){transform_state=sm;}                                                  ///<设置场景矩阵

        void SetLocalNormal (const Vector3f &nor){transform_state.SetLocalNormal(nor);}                                         ///<设置本地法线
        void SetLocalMatrix (const Matrix4f &mat){transform_state.SetLocalMatrix(mat);}                                         ///<设置本地矩阵
        void SetParentMatrix(const Matrix4f &mat){transform_state.SetParentMatrix(mat);}                                        ///<设置上级到世界空间变换矩阵

    public:

        const TransformState &GetTransformState                   ()const {return transform_state;}                          ///<取得场景矩阵

        const uint32          GetTransformVersion                 ()const {return transform_state.GetNewestVersion();}       ///<取得变换矩阵版本号

        const Vector3f &      GetWorldPosition                    ()const {return transform_state.GetWorldPosition();}       ///<取得世界坐标
        const Matrix4f &      GetLocalMatrix                      ()const {return transform_state.GetLocalMatrix();}         ///<取得本地矩阵

        TransformManager &    GetTransform                        ()      {return transform_state.GetTransform();}           ///<取得变换管理器
        void                  ClearTransforms                     ()      {       transform_state.GetTransform().Clear();}   ///<清空变换列表

        const Matrix4f &      GetLocalToWorldMatrix               ()      {return transform_state.GetLocalToWorldMatrix();}  ///<取得本地到世界矩阵
        const Matrix4f &      GetWorldToLocalMatrix               ()      {return transform_state.GetWorldToLocalMatrix();}
        const Matrix4f &      GetNormalMatrix                     ()      {return transform_state.GetNormalMatrix();}

    public:

        virtual void UpdateWorldTransform();
    };//class NodeTransform
}//namespace hgl::graph
