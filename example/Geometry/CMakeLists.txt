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

CreateProject(ExtrudedPolygonTest               ExtrudedPolygonTest.cpp)

CreateProject(WallsFromPolyline                 WallsFromPolyline.cpp)
#CreateProject(WallsFromPolyline_Texture        WallsFromPolyline_Texture.cpp)

CreateProject(LineRenderTest                    LineRenderTest.cpp)


add_subdirectory(LoadGeometry)
add_subdirectory(LoadScene)
