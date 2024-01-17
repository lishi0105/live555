include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg
)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SERVER_SRCS)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg SERVER_SRCS)

set(SERVER_TARGET "live-server")

add_executable(
    ${SERVER_TARGET}
    ${SERVER_SRCS}
)

# 链接库
target_link_libraries(
    ${SERVER_TARGET}
    pthread m dl z
    LiveRtspServer liveMedia BasicUsageEnvironment groupsock UsageEnvironment 
    ssl crypto
    avformat avcodec avutil swresample 
    X11 
)

add_custom_command(
    TARGET ${SERVER_TARGET} POST_BUILD
    COMMAND ${CROSS}strip ${SERVER_TARGET}
    COMMAND echo "${SERVER_TARGET} Generating success..."
)