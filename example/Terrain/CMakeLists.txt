add_executable(TerrainHeightmap terrain_heightmap.cpp)

target_link_libraries(TerrainHeightmap ${ULRE})

set_target_properties(TerrainHeightmap PROPERTIES FOLDER "example/terrain")