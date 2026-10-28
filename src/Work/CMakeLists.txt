SET(WORK_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/framework)

SET(WORK_SOURCE	${WORK_INCLUDE_PATH}/WorkObject.h
                ${WORK_INCLUDE_PATH}/WorkManager.h
                ${WORK_INCLUDE_PATH}/AppFramework.h
                ${WORK_INCLUDE_PATH}/LoadedFile.h
                ${WORK_INCLUDE_PATH}/LoadedFileHandler.h
                ${WORK_INCLUDE_PATH}/LoadedFileManager.h
                AppFramework.cpp
                WorkObject.cpp
                WorkManager.cpp
                LoadedFile.cpp
                LoadedFileManager.cpp)

add_cm_library(ULRE.Work "ULRE" ${WORK_SOURCE})