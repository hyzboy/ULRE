macro(CreateProject name main_file)
	add_executable(${name} ${main_file}.cpp)
	target_link_libraries(${name} ${ULRE})
endmacro()

CreateProject(0.triangle main)
CreateProject(1.indices_rect indices_rect)
