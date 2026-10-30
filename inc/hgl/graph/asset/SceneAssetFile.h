#pragma once

#include <hgl/graph/asset/SceneAssetSpec.h>

namespace hgl::graph
{
    bool LoadSceneSpecFromTOML(const OSString &filename,
                               SceneSpec &out_spec);

    bool ParseSceneSpec(const char *content,
                        size_t size,
                        const os_char *debug_name,
                        SceneSpec &out_spec);
}
