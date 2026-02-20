macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Environment")
endmacro()

CreateProject(01_AtmosphereSkyMinimal           AtmosphereSkyMinimal.cpp)

CreateProject(02_AtmosphereSkySunGizmo          AtmosphereSkySunGizmo.cpp
                                            ../Gizmo/Gizmo.h
                                            ../Gizmo/GizmoResource.h
                                            ../Gizmo/GizmoResource.cpp
                                            ../Gizmo/Gizmo3DMove.cpp
                                            ../Gizmo/Gizmo3DScale.cpp
                                            ../Gizmo/Gizmo3DRotate.cpp
                                            ../Gizmo/GizmoUnified.cpp)


