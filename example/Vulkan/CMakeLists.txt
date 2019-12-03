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
CreateProject(02.1.texture_rect texture_rect.cpp)
#CreateProject(02.2.texture_linear texture_linear.cpp TGATexture.cpp)
CreateProject(03.HQFilterTexture HQFilterTexture.cpp TGATexture.cpp)
CreateProject(04.Geometry2D Geometry2D.cpp)
CreateProject(05.Geometry3D Geometry3D.cpp)
CreateProject(06.SceneTree SceneTree.cpp)

CreateProject(07.LoadModel LoadModel.cpp TGATexture.cpp AssimpLoaderMesh.h AssimpLoaderMesh.cpp ViewModelFramework.h)
target_link_libraries(07.LoadModel assimp)

CreateProject(08.InlineGeometryScene InlineGeometryScene.cpp)

CreateProject(09.Atomsphere Atomsphere.cpp)

CreateProject(10.PBRBasic PBRBasic.cpp)

CreateProject(11.Deferred Deferred.cpp TGATexture.cpp)

#CreateProject(12.DeferredModel DeferredModel.cpp TGATexture.cpp)

CreateProject(13.TextureFormat TextureFormat.cpp)
