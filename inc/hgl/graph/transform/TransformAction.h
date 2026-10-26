/**
 * ULRE SceneGraph transform action composition.
 *
 * This runtime layer depends on CMCore; the pure TRS Transform remains in CMMath.
 */

#pragma once
#include<hgl/type/TypeInfo.h>
#include<hgl/math/VectorOperations.h>
#include<hgl/math/VectorTypes.h>
#include<hgl/math/Matrix.h>
#include<hgl/math/Quaternion.h>
#include<hgl/math/Projection.h>
#include<hgl/type/ManagedArray.h>
#include<hgl/type/VersionData.h>

namespace hgl::math
{
    /**
    * 变换基类
    */
    class TransformAction:public VersionData<Matrix4f>
    {
    protected:

        Vector3f WorldPosition;
        Vector3f WorldNormal;

    protected:

        virtual void MakeNewestData(Matrix4f &)=0;

    public:

        TransformAction():VersionData(Identity4f), WorldPosition(ZeroVector3f), WorldNormal(ZeroVector3f){}
        TransformAction(const TransformAction * const tb):VersionData(tb)
        {
            if(tb)
            {
                WorldPosition=tb->WorldPosition;
                WorldNormal=tb->WorldNormal;
            }
            else
            {
                WorldPosition=ZeroVector3f;
                WorldNormal=ZeroVector3f;
            }
        }
        virtual ~TransformAction()=default;

        virtual constexpr const size_t GetTypeHash()const=0;                ///<取得类型哈希值

        const uint32 GetMatrix(Matrix4f &mat,const Vector3f &wp,const Vector3f &wn)            ///<取得当前矩阵，并返回当前矩阵版本号
        {
            WorldPosition=wp;
            WorldNormal=wn;

            return GetNewestVersionData(mat);
        }

        virtual TransformAction *CloneSelf()const=0;                     ///<创建一个自身的复制

        virtual bool Update(){return false;}
    };//class TransformAction

    class TransActionMatrix :public TransformAction
    {
        Matrix4f transform_matrix;

    protected:

        void MakeNewestData(Matrix4f &mat) override
        {
            mat=transform_matrix;
        }

    public:

        constexpr const size_t GetTypeHash()const override { return hgl::GetTypeHash<TransActionMatrix>(); }

    public:

        TransActionMatrix():TransformAction()
        {
            transform_matrix=Identity4f;
            UpdateVersion();
        }

        TransActionMatrix(const Matrix4f &mat):TransformAction()
        {
            transform_matrix=mat;
            UpdateVersion();
        }

        TransActionMatrix(const TransActionMatrix *tm):TransformAction(tm)
        {
            transform_matrix=tm->transform_matrix;
            UpdateVersion();
        }

        TransformAction *CloneSelf()const override
        {
            return(new TransActionMatrix(this));
        }

        const Matrix4f &GetTransformMatrix()const { return transform_matrix; }

        void SetTransformMatrix(const Matrix4f &mat)
        {
            if (IsNearlyEqual(transform_matrix, mat))
                return;

            transform_matrix=mat;
            UpdateVersion();
        }
    };//class TransActionMatrix

    class TransActionTranslate :public TransformAction
    {
        Vector3f offset;

    protected:

        void MakeNewestData(Matrix4f &mat) override
        {
            mat=TranslateMatrix(offset);
        }

    public:

        constexpr const size_t GetTypeHash()const override{return hgl::GetTypeHash<TransActionTranslate>();}

    public:

        TransActionTranslate():TransformAction()
        {
            offset=ZeroVector3f;
            UpdateVersion();
        }

        TransActionTranslate(const Vector3f &o):TransformAction()
        {
            offset=o;
            UpdateVersion();
        }

        TransActionTranslate(const TransActionTranslate *tt):TransformAction(tt)
        {
            offset=tt->offset;
            UpdateVersion();
        }

        TransformAction *CloneSelf()const override
        {
            return(new TransActionTranslate(this));
        }

        const Vector3f &GetOffset()const { return offset; }
        void SetOffset(const Vector3f &o)
        {
            if(IsNearlyEqual(offset,o))
                return;

            offset=o;
            UpdateVersion();
        }

        void AddOffset(const Vector3f &o)
        {
            if (IsNearlyZero(o))
                return;

            offset+=o;
            UpdateVersion();
        }
    };//class TransActionTranslate

    using TransActionMove=TransActionTranslate;

    class TransActionRotateQuat :public TransformAction
    {
        Quatf quat;

    protected:

        void MakeNewestData(Matrix4f &mat) override
        {
            mat=ToMatrix(quat);
        }

    public:

        constexpr const size_t GetTypeHash()const override{return hgl::GetTypeHash<TransActionRotateQuat>();}

    public:

        TransActionRotateQuat():TransformAction()
        {
            quat=IdentityQuatf;
            UpdateVersion();
        }

        TransActionRotateQuat(const Quatf &q):TransformAction()
        {
            quat=q;
            UpdateVersion();
        }

        TransActionRotateQuat(const TransActionRotateQuat *trq):TransformAction(trq)
        {
            quat=trq->quat;
            UpdateVersion();
        }

        TransformAction *CloneSelf()const override
        {
            return(new TransActionRotateQuat(quat));
        }

        const Quatf &GetQuat()const { return quat; }
        void SetQuat(const Quatf &q)
        {
            if(IsNearlyEqual(quat,q))
                return;

            quat=q;
            UpdateVersion();
        }
    };//class TransActionRotateQuat

    class TransActionRotateAxis :public TransformAction
    {
        Vector3f axis;
        float angle;

    protected:

        void MakeNewestData(Matrix4f &mat) override
        {
            mat=AxisRotate(angle,axis);
        }

    public:

        constexpr const size_t GetTypeHash()const override { return hgl::GetTypeHash<TransActionRotateAxis>(); }

    public:

        TransActionRotateAxis():TransformAction()
        {
            axis=AxisVector::Z; // default to unit Z axis to avoid using zero vector
            angle=0;
            UpdateVersion();
        }

        TransActionRotateAxis(const Vector3f &a,float ang):TransformAction()
        {
            axis=a;
            angle=ang;
            UpdateVersion();
        }

        TransActionRotateAxis(const TransActionRotateAxis *tra):TransformAction(tra)
        {
            axis=tra->axis;
            angle=tra->angle;
            UpdateVersion();
        }

        TransformAction *CloneSelf()const override
        {
            return(new TransActionRotateAxis(axis,angle));
        }

        const Vector3f &GetAxis()const { return axis; }
        const float GetAngle()const { return angle; }

        void SetAxisAngle(const Vector3f &a,float ang)
        {
            if (IsNearlyEqual(axis,a) && IsNearlyEqual(angle,ang))
                return;

            axis=a;
            angle=ang;
            UpdateVersion();
        }

        void SetAxis(const Vector3f &a)
        {
            if (IsNearlyEqual(axis,a))
                return;

            axis=a;
            UpdateVersion();
        }

        void SetAngle(float a)
        {
            if (IsNearlyEqual(angle,a))
                return;

            angle=a;
            UpdateVersion();
        }
    };//class TransActionRotateAxis

    class TransActionRotateEuler :public TransformAction
    {
        Vector3f euler;

    protected:

        void MakeNewestData(Matrix4f &mat) override
        {
            mat=glm::eulerAngleXYZ(euler.x,euler.y,euler.z);
        }

    public:

        constexpr const size_t GetTypeHash()const override { return hgl::GetTypeHash<TransActionRotateEuler>(); }

    public:

        TransActionRotateEuler():TransformAction()
        {
            euler=ZeroVector3f;
            UpdateVersion();
        }

        TransActionRotateEuler(const Vector3f &e):TransformAction()
        {
            euler=e;
            UpdateVersion();
        }

        TransActionRotateEuler(const TransActionRotateEuler *tre):TransformAction(tre)
        {
            euler=tre->euler;
            UpdateVersion();
        }

        TransformAction *CloneSelf()const override
        {
            return(new TransActionRotateEuler(euler));
        }

        const Vector3f &GetEuler()const { return euler; }

        const float     GetPitch()const { return euler.x; }
        const float     GetYaw  ()const { return euler.y; }
        const float     GetRoll ()const { return euler.z; }

        void SetEuler   (const Vector3f &e  ){if(IsNearlyEqual(euler,  e))return;euler  =e;UpdateVersion();}
        void SetPitch   (const float p      ){if(IsNearlyEqual(euler.x,p))return;euler.x=p;UpdateVersion();}
        void SetYaw     (const float y      ){if(IsNearlyEqual(euler.y,y))return;euler.y=y;UpdateVersion();}
        void SetRoll    (const float r      ){if(IsNearlyEqual(euler.z,r))return;euler.z=r;UpdateVersion();}
    };//class TransActionRotateEuler

    class TransActionScale :public TransformAction
    {
        Vector3f scale3f;

    protected:

        void MakeNewestData(Matrix4f &mat) override
        {
            mat=ScaleMatrix(scale3f);
        }

    public:

        constexpr const size_t GetTypeHash()const override { return hgl::GetTypeHash<TransActionScale>(); }

    public:

        TransActionScale():TransformAction()
        {
            scale3f=Vector3f(1,1,1);
            UpdateVersion();
        }

        TransActionScale(const Vector3f &s):TransformAction()
        {
            scale3f=s;
            UpdateVersion();
        }

        TransActionScale(const TransActionScale *ts):TransformAction(ts)
        {
            scale3f=ts->scale3f;
            UpdateVersion();
        }

        TransformAction *CloneSelf()const override
        {
            return(new TransActionScale(scale3f));
        }

        const Vector3f &GetScale()const { return scale3f; }
        void SetScale(const Vector3f &s)
        {
            if (IsNearlyEqual(scale3f,s))
                return;

            scale3f=s;
            UpdateVersion();
        }
    };//class TransformScale

    class TransActionLookAt :public TransformAction
    {
        Vector3f eye;
        Vector3f center;
        Vector3f up;

    protected:

        void MakeNewestData(Matrix4f &mat) override
        {
            mat=LookAtMatrix(eye,center,up);
        }

    public:

        constexpr const size_t GetTypeHash()const override { return hgl::GetTypeHash<TransActionLookAt>(); }

    public:

        TransActionLookAt():TransformAction()
        {
            eye     =OneVector3f;
            center  =ZeroVector3f;
            up      =AxisVector::Z;
            UpdateVersion();
        }

        TransActionLookAt(const Vector3f &e,const Vector3f &c,const Vector3f &u=AxisVector::Z):TransformAction()
        {
            eye     =e;
            center  =c;
            up      =u;
            UpdateVersion();
        }

        TransActionLookAt(const TransActionLookAt *tla):TransformAction(tla)
        {
            eye     =tla->eye;
            center  =tla->center;
            up      =tla->up;
            UpdateVersion();
        }

        TransformAction *CloneSelf()const override
        {
            return(new TransActionLookAt(eye,center,up));
        }

        const Vector3f &GetEye()const { return eye; }
        const Vector3f &GetCenter()const { return center; }
        const Vector3f &GetUp()const { return up; }

        void SetLookAt(const Vector3f &e,const Vector3f &c,const Vector3f &u)
        {
            if (IsNearlyEqual(eye,e) && IsNearlyEqual(center,c) && IsNearlyEqual(up,u))
                return;

            eye=e;
            center=c;
            up=u;
            UpdateVersion();
        }

        void SetEye(const Vector3f &pos)
        {
            if(IsNearlyEqual(eye,pos))
                return;

            eye=pos;
            UpdateVersion();
        }

        void SetCenter(const Vector3f &pos)
        {
            if (IsNearlyEqual(center,pos))
                return;

            center=pos;
            UpdateVersion();
        }

        void SetUp(const Vector3f &pos)
        {
            if (IsNearlyEqual(up,pos))
                return;

            up=pos;
            UpdateVersion();
        }
    };//class TransActionLookAt

    class TransActionManager: public TransformAction
    {
        ManagedArray<TransformAction> transform_list;

    protected:

        virtual void MakeNewestData(Matrix4f &mat) override
        {
            mat=Identity4f;

            if (transform_list.IsEmpty())
                return;

            Matrix4f TempMatrix;

            for(TransformAction *tb:transform_list)
            {
                tb->GetMatrix(TempMatrix,WorldPosition,WorldNormal);

                mat*=TempMatrix;
            }
        }

    public:

        virtual constexpr const size_t GetTypeHash()const override { return hgl::GetTypeHash<TransActionManager>(); }

        const bool IsEmpty()const { return transform_list.IsEmpty(); }

    public:

        TransActionManager()=default;
        virtual ~TransActionManager()=default;

        TransActionManager(const TransActionManager *tm):TransformAction(tm)
        {
            for(const TransformAction *tb:tm->transform_list)
                AddTransform(tb->CloneSelf());

            UpdateVersion();
        }

        TransformAction *CloneSelf()const override
        {
            TransActionManager *tm=new TransActionManager;

            for(TransformAction *tb:transform_list)
                tm->AddTransform(tb->CloneSelf());

            return tm;
        }

        void operator = (const TransActionManager &tm)
        {
            Clear();

            for(TransformAction *tb:tm.transform_list)
                AddTransform(tb->CloneSelf());
        }

        void Clear()
        {
            transform_list.Clear();
            UpdateVersion();
        }

        void AddTransform(TransformAction *tb)
        {
            transform_list.Add(tb);
            UpdateVersion();
        }

        TransActionTranslate *AddTranslate(const Vector3f &v)
        {
            TransActionTranslate *tt=new TransActionTranslate(v);

            AddTransform(tt);

            return tt;
        }

        TransActionMove *AddMove(const Vector3f &v){return AddTranslate(v); }

        TransActionRotateAxis *AddRotateAxis(const Vector3f &axis,const float angle)
        {
            TransActionRotateAxis *tra=new TransActionRotateAxis(axis,angle);

            AddTransform(tra);

            return tra;
        }

        TransActionRotateQuat *AddRotateQuat(const Quatf &q)
        {
            TransActionRotateQuat *trq=new TransActionRotateQuat(q);

            AddTransform(trq);

            return trq;
        }

        TransActionRotateEuler *AddRotateEuler(const Vector3f &euler)
        {
            TransActionRotateEuler *tre=new TransActionRotateEuler(euler);

            AddTransform(tre);

            return tre;
        }

        TransActionScale *AddScale(const Vector3f &v)
        {
            TransActionScale *ts=new TransActionScale(v);

            AddTransform(ts);

            return ts;
        }

        TransActionLookAt *AddLookAt(const Vector3f &eye,const Vector3f &center,const Vector3f &up)
        {
            TransActionLookAt *tla=new TransActionLookAt;

            tla->SetLookAt(eye,center,up);

            AddTransform(tla);

            return tla;
        }

        TransActionMatrix *AddMatrix(const Matrix4f &mat)
        {
            TransActionMatrix *tm=new TransActionMatrix(mat);

            AddTransform(tm);

            return tm;
        }

        void RemoveTransform(TransformAction *tb)
        {
            if(!tb)
                return;

            const int pos=transform_list.Find(tb);

            if(pos<0)
                return;

            transform_list.DeleteShift(pos);     //使用DeleteShift是为了保持顺序(Delete可能会拿最后一个数据移到前面，而本类需要保持顺序)

            UpdateVersion();
        }

        virtual bool Update() override
        {
            bool has_update=false;

            for(TransformAction *tb:transform_list)
                if(tb->UpdateNewestData())
                    has_update=true;

            if(has_update)
            {
                UpdateVersion();
                UpdateNewestData();
            }

            return has_update;
        }
    };//class TransActionManager
}//namespace hgl::math
