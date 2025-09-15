#pragma once

#include<hgl/graph/pipeline/VKPipelineData.h>
#include<hgl/util/hash/Hash.h>

VK_NAMESPACE_BEGIN

constexpr const hgl::util::HASH PipelineHash=hgl::util::HASH::xxH3_128;

using PipelineHashCode=hgl::util::HashCode<128/8>;

VK_NAMESPACE_END
