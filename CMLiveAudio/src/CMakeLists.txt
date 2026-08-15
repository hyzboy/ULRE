set(CM_LIVE_AUDIO_HEADER   ${CMLIVEAUDIO_ROOT_INCLUDE_PATH}/hgl/audio/live/LiveAudio.h)

set(CM_LIVE_AUDIO_SOURCE
    LiveAudio.cpp)

source_group("LiveAudio" FILES ${CM_LIVE_AUDIO_HEADER}
                                ${CM_LIVE_AUDIO_SOURCE})

add_cm_library(CMLiveAudio "CM" ${CMLIVEAUDIO_ROOT_INCLUDE_PATH} ${CM_LIVE_AUDIO_HEADER}
                                ${CM_LIVE_AUDIO_SOURCE})

target_link_libraries(CMLiveAudio PRIVATE CMAudio)
