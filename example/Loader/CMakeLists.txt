# LoadedFileManager 相关测试
# 工作目录设为仓库根目录：测试既读写临时文件，也访问 res/image 下的真实资产。

add_executable(LoadedFileManagerTest LoadedFileManagerTest.cpp)
target_link_libraries(LoadedFileManagerTest ULRE::Engine)

if(MSVC)
    set_target_properties(LoadedFileManagerTest PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

set_property(TARGET LoadedFileManagerTest PROPERTY FOLDER "ULRE/Example/Loader")

if(BUILD_TESTING)
    add_test(
        NAME LoadedFileManagerTest
        COMMAND LoadedFileManagerTest
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(LoadedFileManagerTest PROPERTIES TIMEOUT 60)
endif()