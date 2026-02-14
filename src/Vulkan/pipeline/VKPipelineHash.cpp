#include<hgl/vk/pipeline/VKPipelineHash.h>
#include<hgl/util/hash/Hash.h>
#include<hgl/io/MemoryOutputStream.h>
#include<hgl/io/DataOutputStream.h>

VK_NAMESPACE_BEGIN

const bool CountHash(PipelineHashCode *hash_code,const PipelineData *pd)
{
    if(!hash_code||!pd)return(false);

    io::MemoryOutputStream mos;
    AutoDelete<io::DataOutputStream> dos=new io::LEDataOutputStream(&mos);

    if(!pd->SaveToStream(dos))
        return(false);

    return hgl::util::hash::Hash(PipelineHash, mos.GetData(), mos.Tell(), hash_code->code);
}

VK_NAMESPACE_END
