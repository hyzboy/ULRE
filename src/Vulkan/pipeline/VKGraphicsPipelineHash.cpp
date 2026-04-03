#include<hgl/vk/pipeline/VKGraphicsPipelineHash.h>
#include<hgl/util/hash/Hash.h>
#include<hgl/io/MemoryOutputStream.h>
#include<hgl/io/DataOutputStream.h>

namespace hgl::graph{

const bool CountHash(GraphicsPipelineHashCode *hash_code,const GraphicsPipelineData *pd)
{
    if(!hash_code||!pd)return(false);

    io::MemoryOutputStream mos;
    AutoDelete<io::DataOutputStream> dos=new io::LEDataOutputStream(&mos);

    if(!pd->SaveToStream(dos))
        return(false);

    return hgl::util::hash::Hash(GraphicsPipelineHash, mos.GetData(), mos.Tell(), hash_code->code);
}

}//namespace hgl::graph
