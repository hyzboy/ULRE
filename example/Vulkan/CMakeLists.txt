macro(CreateProject name)
	add_executable(${name} ${ARGN} VulkanAppFramework.h)
	target_link_libraries(${name} ${ULRE})
    
    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()
endmacro()

CreateProject(0.triangle main.cpp)
CreateProject(1.indices_rect indices_rect.cpp)
CreateProject(2.texture_rect texture_rect.cpp TGATexture.cpp)
CreateProject(3.Geometry2D Geometry2D.cpp)
CreateProject(4.Geometry3D Geometry3D.cpp)
CreateProject(5.SceneTree SceneTree.cpp)

CreateProject(6.LoadModel LoadModel.cpp TGATexture.cpp AssimpLoaderMesh.h AssimpLoaderMesh.cpp ViewModelFramework.h)
target_link_libraries(6.LoadModel assimp)

CreateProject(7.InlineGeometryScene InlineGeometryScene.cpp)

CreateProject(8.Atomsphere Atomsphere.cpp)

CreateProject(9.Deferred Deferred.cpp TGATexture.cpp)

CreateProject(10.TextureFormat TextureFormat.cpp TGATexture.cpp)