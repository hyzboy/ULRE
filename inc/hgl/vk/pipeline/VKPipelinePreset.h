#pragma once

#include<hgl/graph/PipelinePreset.h>

namespace hgl::graph{

struct PipelineData;

/**
 * 获取内置管线数据
 */
const PipelineData *GetPipelineData(const PipelinePreset &);
}//namespace hgl::graph
