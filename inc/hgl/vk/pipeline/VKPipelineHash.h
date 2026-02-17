#pragma once

#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/util/hash/Hash.h>

namespace hgl::graph{

constexpr const hgl::util::hash::Algorithm PipelineHash=hgl::util::hash::Algorithm::XXH3_128;

using PipelineHashCode=hgl::util::hash::Digest<128/8>;

}//namespace hgl::graph
