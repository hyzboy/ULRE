macro(set_compiler_param)

    IF(WIN32)

        SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MDd")
        SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")

        SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MD")
        SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")

        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14")

        add_compile_options(/arch:AVX2)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)

    ELSE()
        IF(NOT ANDROID)
            IF(APPLE)
                SET(USE_CLANG       ON)
            ELSE()
                OPTION(USE_CLANG    OFF)
            ENDIF()

            if(USE_CLANG)
                SET(CMAKE_C_COMPILER clang)
                SET(CMAKE_CXX_COMPILER clang++)
            endif()
        ENDIF()

        add_compile_options(-mavx)

        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")

        SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb3")
        SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3")

        SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Ofast")
        SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")
    ENDIF()

    MESSAGE("C Compiler: " ${CMAKE_C_COMPILER})
    MESSAGE("C++ Compiler: " ${CMAKE_CXX_COMPILER})
    MESSAGE("C Flag: " ${CMAKE_C_FLAGS})
    MESSAGE("C++ Flag: " ${CMAKE_CXX_FLAGS})

endmacro()
