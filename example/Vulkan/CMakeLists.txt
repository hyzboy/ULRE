macro(CreateProject name)
    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Vulkan")
endmacro()

macro(CreateQtProject name)
    add_executable(${name} ${ARGN} VulkanAppFramework.h)

    set(IDENTIFIER "com.hyzgame.example")

    target_link_libraries(${name} ${ULRE} Qt5::Core Qt5::Gui Qt5::Widgets)

    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Vulkan")
endmacro()

CreateProject(00.triangle first_triangle.cpp)
CreateProject(01.two_triangle two_triangle.cpp)
CreateProject(02.FragCoord FragCoordTest.cpp)
CreateProject(03.indices_rect indices_rect.cpp)
CreateProject(04.TextureFormat TextureFormat.cpp)
CreateProject(05.texture_rect texture_rect.cpp)
#CreateProject(05.HQFilterTexture HQFilterTexture.cpp)
#CreateProject(06.Geometry2D Geometry2D.cpp)
CreateProject(06.Geometry3D Geometry3D.cpp)
#CreateProject(08.SceneTree SceneTree.cpp)
#CreateProject(09.LoadStaticMesh LoadStaticMesh.cpp LoadScene.cpp)
CreateProject(10.InlineGeometryScene InlineGeometryScene.cpp)
CreateProject(11.Atomsphere Atomsphere.cpp)

CreateProject(12.RectanglePrimitive RectanglePrimitive.cpp)

CreateProject(13.DrawTile DrawTile.cpp)

CreateProject(14.DrawText DrawText.cpp)

CreateProject(15.OffscreenRender OffscreenRender.cpp)

#CreateProject(12.PBRBasic PBRBasic.cpp)
#CreateProject(12.Deferred Deferred.cpp)
CreateProject(16.DeferredRenderMultiCmdBuffer DeferredRenderMultiCmdBuffer.cpp)
#CreateProject(14.AutoMaterial auto_material.cpp)

CreateProject(17.Cubemap Cubemap.cpp)
CreateProject(18.RayPicking RayPicking.cpp)