#include<hgl/graph/shader/ShaderMaker.h>
#include<hgl/graph/shader/param/in.h>
#include<hgl/type/Set.h>

BEGIN_SHADER_NAMESPACE
bool ShaderMaker::Check()
{
    if(!fin_node)
        return(false);

    Set<node::Node *> post;             //完成检测的节点合集
    Set<node::Node *> prev;             //等待检测的节点合集
    node::Node *cur,*nn;

    node::InputParamList *ipl;
    int i,count;
    param::InputParam **ip;

    prev.Add(fin_node);

    while(prev.GetEnd(cur))
    {
        ipl     =&(cur->GetInputParamList());
        count   =ipl->GetCount();
        ip      =ipl->GetData();

        for(i=0;i<count;i++)
        {
            if(!(*ip)->Check())
                return(false);

            nn=(*ip)->GetJoinNode();

            if(nn)
            {
                if(!post.IsMember(nn)
                 &&!prev.IsMember(nn)
                 &&nn!=cur)
                {
                    prev.Add(nn);
                }
            }

            ip++;
        }
        
        prev.Delete(cur);
        post.Add(cur);
    }

    return(true);
}

bool ShaderMaker::Make()
{
    if(!Check())
        return(false);


}
END_SHADER_NAMESPACE
