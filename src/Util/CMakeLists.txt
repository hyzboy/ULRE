SET(UTIL_HEADER ${ROOT_INCLUDE_PATH}/hgl/util/JsonTool.h
                ${ROOT_INCLUDE_PATH}/hgl/db/FieldType.h
                ${ROOT_INCLUDE_PATH}/hgl/db/Field.h)

SET(UTIL_SOURCE #Field.cpp
                #FieldTypeConvert.h
                #FieldType.cpp
                JsonTool.cpp)

SOURCE_GROUP("Header Files" FILES ${UTIL_HEADER})
SOURCE_GROUP("Source Files" FILES ${UTIL_SOURCE})

add_cm_library(ULRE.Util "ULRE" ${UTIL_HEADER}
                                ${UTIL_SOURCE})