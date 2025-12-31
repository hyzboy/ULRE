#include<hgl/graph/VKVertexInputLayoutHash.h>

VK_NAMESPACE_BEGIN
/**
* 计算指定配置下需要的流数量
*/
const uint GetVertexInputStreamCount(const VertexInputLayoutHash &vil_hash)
{
    if(vil_hash.hash_code==0)return 0;

    uint count=0;

    if(vil_hash.Position)++count;
    if(vil_hash.Normal)++count;
    if(vil_hash.Tangnet)++count;
    if(vil_hash.Bitangent)++count;

    if(vil_hash.TexCoord)count+=vil_hash.TexCoord;
    if(vil_hash.Color)count+=vil_hash.Color;
    
    if(vil_hash.Bone)count+=2;
    if(vil_hash.Assign)count+=1;

    return count;
}
VK_NAMESPACE_END
