file(GLOB TYPE_TEMPLATE_HEADER ${ROOT_INCLUDE_PATH}/hgl/type/*.h)
file(GLOB TYPE_TEMPLATE_SOURCE ${ROOT_INCLUDE_PATH}/hgl/type/*.cpp)

SET(SYSTEM_INFO_SOURCE   SystemInfo.cpp)

SET(BASE_TYPE_SOURCE
#    DataType/Info.cpp
#    DataType/BitArray.cpp
    DataType/Color.cpp
    DataType/Color3f.cpp
    DataType/Color4f.cpp
#     DataType/FontInfo.cpp
    DataType/StringList.cpp
	DataType/Endian.cpp)

SOURCE_GROUP("DataType\\Template\\Header Files" FILES ${TYPE_TEMPLATE_HEADER})
SOURCE_GROUP("DataType\\Template\\Source Files" FILES ${TYPE_TEMPLATE_SOURCE})
SOURCE_GROUP("DataType" FILES ${BASE_TYPE_SOURCE})

SET(BASE_IO_SOURCE
    IO/DataInputStream.cpp
    IO/DataOutputStream.cpp
    IO/FileAccess.cpp
    IO/FileInputStream.cpp
    IO/FileOutputStream.cpp
    IO/IOType.cpp
    IO/RandomAccessFile.cpp
    IO/TextOutputStream.cpp)

SOURCE_GROUP("IO" FILES ${BASE_IO_SOURCE})

SET(BASE_FILE_SYSTEM_SOURCE
    FileSystem/FileSystem.cpp
    FileSystem/EnumFile.cpp)

SOURCE_GROUP("FileSystem" FILES ${BASE_FILE_SYSTEM_SOURCE})

SET(BASE_OTHER_SOURCE
    Other/CodePage.cpp
    Other/DateTime.cpp
#    Other/ExpendString.cpp
    Other/ThreadFunc.cpp
    Other/TimeCount.cpp
    Other/TimeVal.cpp)

SOURCE_GROUP("Other"                FILES ${BASE_OTHER_SOURCE})

SET(BASE_PLUG_IN_SOURCE
    PlugIn/ExternalModule.cpp
#     PlugIn/PlugIn.cpp
)

SOURCE_GROUP("PlugIn" FILES ${BASE_PLUG_IN_SOURCE})

add_library(ULRE.Base STATIC    #${SYSTEM_INFO_SOURCE}
								${TYPE_TEMPLATE_HEADER}
								${TYPE_TEMPLATE_SOURCE}
								${BASE_TYPE_SOURCE}
                                ${BASE_IO_SOURCE}
                                ${BASE_FILE_SYSTEM_SOURCE}
                                ${BASE_OTHER_SOURCE}
                                ${BASE_PLUG_IN_SOURCE})

set_property(TARGET ULRE.Base PROPERTY FOLDER "ULRE")