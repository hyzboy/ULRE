macro(CreateProject name)
    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Gizmo")
endmacro()

CreateProject(01_PlaneGrid3D PlaneGrid3D.cpp)
CreateProject(02_RayPicking RayPicking.cpp)
CreateProject(03_MetricCellsGrid MetricCellsGrid.cpp)

CreateProject(04_Gizmo3DTest GizmoTest.cpp Gizmo3DMove.cpp Gizmo.h GizmoResource.h GizmoResource.cpp)

#CreateProject(03_BlenderAxis BlenderAxis.cpp)
