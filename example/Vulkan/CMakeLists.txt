macro(CreateProject name)
    add_executable(${name} ${ARGN} VulkanAppFramework.h)
    target_link_libraries(${name} ${ULRE})

    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Vulkan")
endmacro()

macro(CreateQtProject name)
    add_executable(${name} ${ARGN} VulkanAppFramework.h)

    set(IDENTIFIER "com.hyzgame.texconv")

    target_link_libraries(${name} ${ULRE} Qt5::Core Qt5::Gui Qt5::Widgets)

    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Vulkan")
endmacro()

CreateProject(00.triangle first_triangle.cpp)
CreateProject(01.FragCoord FragCoordTest.cpp)
CreateProject(02.indices_rect indices_rect.cpp)
CreateProject(03.TextureFormat TextureFormat.cpp)
CreateProject(04.texture_rect texture_rect.cpp)
CreateProject(05.HQFilterTexture HQFilterTexture.cpp)
CreateProject(06.Geometry2D Geometry2D.cpp)
CreateProject(07.Geometry3D Geometry3D.cpp)
CreateProject(08.SceneTree SceneTree.cpp)
CreateProject(09.LoadStaticMesh LoadStaticMesh.cpp LoadScene.cpp)
CreateProject(10.InlineGeometryScene InlineGeometryScene.cpp)
CreateProject(11.Atomsphere Atomsphere.cpp)

CreateProject(12.RectanglePrimitive RectanglePrimitive.cpp)

CreateProject(13.DrawTile DrawTile.cpp)

CreateProject(14.DrawText DrawText.cpp)

IF(SUPPORT_QT_VULKAN)
include(QtCommon)
CreateQtProject(14.VulkanQT VulkanQtApp.cpp 
                            QtVulkanWindow.cpp
                            QtVulkanWindow.h
                            QtVulkanMainWindow.h
                            QtVulkanMainWindow.cpp)
ENDIF(SUPPORT_QT_VULKAN)

#CreateProject(12.PBRBasic PBRBasic.cpp)
#CreateProject(12.Deferred Deferred.cpp)
#CreateProject(13.DeferredModel DeferredModel.cpp)
#CreateProject(14.AutoMaterial auto_material.cpp)

