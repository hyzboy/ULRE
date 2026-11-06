#pragma once

#include <hgl/graph/asset/LevelSpec.h>

namespace hgl::graph
{
    bool LoadSceneSpecFromFile(const OSString &filename,
                               LevelSpec &out_spec);
    bool LoadSceneSpecFromTOML(const OSString &filename,
                               LevelSpec &out_spec);
    bool LoadSceneSpecFromFlatBuffers(const OSString &filename,
                                      LevelSpec &out_spec);
    bool SaveSceneSpecToFlatBuffers(const LevelSpec &spec,
                                    const OSString &filename);

    bool ParseSceneSpec(const char *content,
                        size_t size,
                        const os_char *debug_name,
                        LevelSpec &out_spec);
    bool ParseSceneSpecFromFlatBuffers(const void *data,
                                       size_t size,
                                       const os_char *debug_name,
                                       LevelSpec &out_spec);
}
