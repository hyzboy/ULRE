macro(CreateProject name)
	add_executable(${name} ${ARGN} VulkanAppFramework.h)
	target_link_libraries(${name} ${ULRE})
    
    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()
endmacro()

CreateProject(00.triangle main.cpp)
CreateProject(01.indices_rect indices_rect.cpp)
CreateProject(02.texture_rect texture_rect.cpp TGATexture.cpp)
CreateProject(03.Geometry2D Geometry2D.cpp)
CreateProject(04.Geometry3D Geometry3D.cpp)
CreateProject(05.SceneTree SceneTree.cpp)

CreateProject(06.LoadModel LoadModel.cpp TGATexture.cpp AssimpLoaderMesh.h AssimpLoaderMesh.cpp ViewModelFramework.h)
target_link_libraries(06.LoadModel assimp)

CreateProject(07.InlineGeometryScene InlineGeometryScene.cpp)

CreateProject(08.Atomsphere Atomsphere.cpp)

CreateProject(09.Deferred Deferred.cpp TGATexture.cpp)

CreateProject(10.TextureFormat TextureFormat.cpp TGATexture.cpp)