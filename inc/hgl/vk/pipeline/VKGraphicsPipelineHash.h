#pragma once

#include<hgl/vk/pipeline/VKGraphicsPipelineData.h>
#include<hgl/util/hash/Hash.h>

namespace hgl::graph{

constexpr const hgl::util::hash::Algorithm GraphicsPipelineHash=hgl::util::hash::Algorithm::XXH3_128;

using GraphicsPipelineHashCode=hgl::util::hash::Digest<128/8>;

}//namespace hgl::graph
