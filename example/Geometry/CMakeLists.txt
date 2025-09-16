macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Geometry")
endmacro()

CreateProject(01_ExtrudedPolygonTest            ExtrudedPolygonTest.cpp)
CreateProject(02_RevolutionPolygonTest          RevolutionPolygonTest.cpp)

CreateProject(03_WallsFromPolyline              WallsFromPolyline.cpp)
#CreateProject(03_WallsFromPolyline_Texture      WallsFromPolyline_Texture.cpp)

CreateProject(04_WireShapeTest                  WireShapeTest.cpp WireShape.cpp LineRenderManager.cpp LineWidthBatch.cpp LineRenderManager.h LineWidthBatch.h)
