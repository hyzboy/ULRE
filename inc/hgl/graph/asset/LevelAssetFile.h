#pragma once

#include <hgl/graph/asset/LevelSpec.h>

namespace hgl::graph
{
    bool LoadLevelSpecFromFile(const OSString &filename,
                               LevelSpec &out_spec);
    bool LoadLevelSpecFromTOML(const OSString &filename,
                               LevelSpec &out_spec);
    bool LoadLevelSpecFromFlatBuffers(const OSString &filename,
                                      LevelSpec &out_spec);
    bool SaveLevelSpecToFlatBuffers(const LevelSpec &spec,
                                    const OSString &filename);

    bool ParseLevelSpec(const char *content,
                        size_t size,
                        const os_char *debug_name,
                        LevelSpec &out_spec);
    bool ParseLevelSpecFromFlatBuffers(const void *data,
                                       size_t size,
                                       const os_char *debug_name,
                                       LevelSpec &out_spec);
}
