add_executable(AssimpImport AssimpImport.cpp AssimpLoader.cpp AssimpLoader.h)

target_link_libraries(AssimpImport  ${ULRE}
									assimp
                                    IrrXML
                                    zlibstatic)

set_property(TARGET AssimpImport PROPERTY FOLDER "ULRE/Tools")
