macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Geometry")
endmacro()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src/SceneGraph/Vulkan)        # For GeometryTest, 未来LoadGeometry.cpp移到SceneGraph目录下，此行即可删除

#CreateProject(ExtrudedPolygonTest               ExtrudedPolygonTest.cpp)

#CreateProject(WallsFromPolyline                 WallsFromPolyline.cpp)
#CreateProject(WallsFromPolyline_Texture        WallsFromPolyline_Texture.cpp)

# Shared/：暂存尚未迁入引擎库的几何加载 TU（LoadGeometry.cpp / LoadStaticMesh.cpp），
#          由 LoadGeometry、LoadScene 两个 target 共用同一份源码（此前两份拷贝完全重复）。
# 线渲染几何
CreateProject(MixedBufferMeshes                  MixedBufferMeshes.cpp)
CreateProject(LineRender                        LineRender.cpp)

add_subdirectory(LoadGeometry)
add_subdirectory(LoadScene)
