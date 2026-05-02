#include<hgl/shadergen/ShaderCreateInfoTask.h>

namespace hgl::graph{

ShaderCreateInfoTask::ShaderCreateInfoTask(MaterialDescriptorDB *m)
    :ShaderCreateInfo(new TaskShaderStageIO(),m)
{
    task_stage_io=static_cast<TaskShaderStageIO *>(stage_io);
}

}//namespace hgl::graph
