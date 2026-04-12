set(SCENEGRAPH_TEST_EXECUTABLES
    test_vertex_type_format_compat
    test_vertex_binding_compat_helper
    test_inline_geometry_compact_format_compat)

foreach(test_name ${SCENEGRAPH_TEST_EXECUTABLES})
    add_executable(${test_name} ${test_name}.cpp)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${test_name} PRIVATE ${ULRE})

    if(BUILD_TESTING)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endif()
endforeach()