add_executable(ModelConvert MainUnit.cpp AssimpLoader.cpp AssimpLoader.h)

target_link_libraries(ModelConvert  ${ULRE}
									assimp
                                    IrrXML
                                    zlibstatic)
