include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src/SceneGraph/Vulkan)        # For GeometryTest

#CreateProject(ExtrudedPolygonTest               ExtrudedPolygonTest.cpp)

#CreateProject(WallsFromPolyline                 WallsFromPolyline.cpp)
#CreateProject(WallsFromPolyline_Texture        WallsFromPolyline_Texture.cpp)

# 线渲染几何
CreateProject(MixedBufferMeshes                  MixedBufferMeshes.cpp)
CreateProject(LineRender                        LineRender.cpp)

add_subdirectory(LoadGeometry)
add_subdirectory(LoadScene)
