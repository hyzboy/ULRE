#ifndef HGL_GRAPH_SHADER_PARAM_INPUT_INCLUDE
#define HGL_GRAPH_SHADER_PARAM_INPUT_INCLUDE

#include<hgl/graph/shader/common.h>
#include<hgl/graph/shader/param/out.h>
#include<hgl/graph/shader/param/param.h>

BEGIN_SHADER_PARAM_NAMESPACE

/**
 * 输入参数定义
 */
class InputParam:public Param
{
    bool must_join;                 //是否必须有接入

protected:

    node::Node *join_node;
    param::OutputParam *join_param;

protected:

    /**
     * 可否接入判断，默认只判断类型
     */
    virtual bool JoinCheck(node::Node *n,param::OutputParam *op)
    {
        return(op->GetType()==this->GetType());
    }

public: //属性

    node::Node *        GetJoinNode (){return join_node;}                       ///<取得接入节点
    param::OutputParam *GetJoinParam(){return join_param;}                      ///<取得接入节点参数

    virtual const UTF8String GetDefaultValue()const{return "?";}                ///<取得缺省值

public:

    InputParam(const bool mj,const UTF8String &n,const ParamType &t):Param(n,t)
    {
        must_join=mj;
        join_node=nullptr;
        join_param=nullptr;
    }
    virtual ~InputParam()=default;

    virtual bool Join(node::Node *,param::OutputParam *);                       ///<增加一个关联节点
    virtual void Break(){join_node=nullptr;join_param=nullptr;}                 ///<断开一个关联节点

    virtual bool Check(){return(must_join?join_param:true);}                    ///<检测当前节点是否可用
};//class InputParam:public Param
END_SHADER_PARAM_NAMESPACE
#endif//#ifndef HGL_GRAPH_SHADER_PARAM_INPUT_INCLUDE
