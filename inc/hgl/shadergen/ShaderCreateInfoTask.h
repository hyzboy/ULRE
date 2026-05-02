#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderStageIO.h>

namespace hgl::graph
{
    class ShaderCreateInfoTask:public ShaderCreateInfo
    {
        TaskShaderStageIO *task_stage_io;

    public:

        TaskShaderStageIO *GetTaskStageIO(){return task_stage_io;}
        const TaskShaderStageIO *GetTaskStageIO()const{return task_stage_io;}

    public:

        ShaderCreateInfoTask(MaterialDescriptorDB *m);
        ~ShaderCreateInfoTask()override=default;
    };//class ShaderCreateInfoTask
}//namespace hgl::graph
