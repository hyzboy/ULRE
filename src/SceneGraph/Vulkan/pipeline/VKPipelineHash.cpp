#include<hgl/graph/pipeline/VKPipelineHash.h>
#include<hgl/util/hash/Hash.h>
#include<hgl/io/MemoryOutputStream.h>
#include<hgl/io/DataOutputStream.h>

VK_NAMESPACE_BEGIN

const bool CountHash(PipelineHashCode *hash_code,const PipelineData *pd)
{
    if(!hash_code||!pd)return(false);

    AutoDelete<hgl::util::Hash> h=hgl::util::CreateHash<PipelineHash>();

    if(!h)return(false);

    h->Init();

    io::MemoryOutputStream mos;
    AutoDelete<io::DataOutputStream> dos=new io::LEDataOutputStream(&mos);

    if(!pd->SaveToStream(dos))
        return(false);

    h->Update(mos.GetData(),mos.Tell());

    h->Final(hash_code->code);

    return(true);
}

VK_NAMESPACE_END
