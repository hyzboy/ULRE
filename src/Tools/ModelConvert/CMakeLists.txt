# AssimpImport = 旧 Assimp 转换器：与现引擎不完全兼容 ⇒ 2026-10-13 屏蔽（不参与默认构建）。
# 需要单独编它时：cmake --build build --target AssimpImport。源码保留、不处理。
add_executable(AssimpImport AssimpImport.cpp AssimpLoader.cpp AssimpLoader.h EXCLUDE_FROM_ALL)

target_link_libraries(AssimpImport  ${ULRE}
									assimp
                                    IrrXML
                                    zlibstatic)

set_property(TARGET AssimpImport PROPERTY FOLDER "ULRE/Tools")
