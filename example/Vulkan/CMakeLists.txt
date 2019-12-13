macro(CreateProject name)
    add_executable(${name} ${ARGN} VulkanAppFramework.h)
    target_link_libraries(${name} ${ULRE})

    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Vulkan")
endmacro()

CreateProject(00.triangle first_triangle.cpp)
CreateProject(01.indices_rect indices_rect.cpp)
CreateProject(02.TextureFormat TextureFormat.cpp)
CreateProject(03.texture_rect texture_rect.cpp)
CreateProject(04.HQFilterTexture HQFilterTexture.cpp)
CreateProject(05.Geometry2D Geometry2D.cpp)
CreateProject(06.Geometry3D Geometry3D.cpp)
CreateProject(07.SceneTree SceneTree.cpp)

CreateProject(08.LoadModel LoadModel.cpp AssimpLoaderMesh.h AssimpLoaderMesh.cpp ViewModelFramework.h)
target_link_libraries(08.LoadModel assimp)

CreateProject(09.InlineGeometryScene InlineGeometryScene.cpp)

CreateProject(10.Atomsphere Atomsphere.cpp)

CreateProject(11.PBRBasic PBRBasic.cpp)

#CreateProject(12.Deferred Deferred.cpp)

CreateProject(12.DeferredModel DeferredModel.cpp)

CreateProject(13.AutoMaterial auto_material.cpp)
