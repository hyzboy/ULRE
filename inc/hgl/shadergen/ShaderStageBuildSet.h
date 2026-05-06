#pragma once

#include<hgl/shadergen/ShaderStageMap.h>

namespace hgl::graph::mtl
{
class ShaderStageBuildSet
{
    ShaderStageMap *shader_map;

public:

    explicit ShaderStageBuildSet(ShaderStageMap &map):shader_map(&map){}

    bool Add(ShaderCreateInfo *sc)
    {
        return shader_map?shader_map->Add(sc):false;
    }

    bool IsEmpty()const
    {
        return !shader_map||shader_map->IsEmpty();
    }

    ShaderCreateInfo *Find(const ShaderStage stage)
    {
        return shader_map?shader_map->Find(stage):nullptr;
    }

    const ShaderCreateInfo *Find(const ShaderStage stage)const
    {
        return shader_map?shader_map->Find(stage):nullptr;
    }

    ShaderStageMap &GetMap()
    {
        return *shader_map;
    }

    const ShaderStageMap &GetMap()const
    {
        return *shader_map;
    }

    void DeleteAllShaders();
};
}//namespace hgl::graph::mtl
