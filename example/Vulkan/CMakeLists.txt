macro(CreateProject name main_file)
	add_executable(${name} ${main_file}.cpp)
	target_link_libraries(${name} ${ULRE})
endmacro()

add_library(TGATexture STATIC TGATexture.cpp)

CreateProject(0.triangle main)
CreateProject(1.indices_rect indices_rect)
CreateProject(2.texture_rect texture_rect)
target_link_libraries(2.texture_rect TGATexture)
CreateProject(3.Geometry2D Geometry2D)
CreateProject(4.Geometry3D Geometry3D)
