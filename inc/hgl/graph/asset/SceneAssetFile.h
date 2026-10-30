#pragma once

#include <hgl/graph/asset/SceneAssetSpec.h>

namespace hgl::graph
{
    bool LoadSceneSpecFromFile(const OSString &filename,
                               SceneSpec &out_spec);
    bool LoadSceneSpecFromTOML(const OSString &filename,
                               SceneSpec &out_spec);
    bool LoadSceneSpecFromFlatBuffers(const OSString &filename,
                                      SceneSpec &out_spec);
    bool SaveSceneSpecToFlatBuffers(const SceneSpec &spec,
                                    const OSString &filename);

    bool ParseSceneSpec(const char *content,
                        size_t size,
                        const os_char *debug_name,
                        SceneSpec &out_spec);
    bool ParseSceneSpecFromFlatBuffers(const void *data,
                                       size_t size,
                                       const os_char *debug_name,
                                       SceneSpec &out_spec);
}
