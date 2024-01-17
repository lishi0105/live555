# 添加当前目录和子目录到包含路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg
    ${CMAKE_CURRENT_SOURCE_DIR}/opengl
)

# 添加当前目录和子目录的源文件
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} DIR_SRCS)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg DIR_SRCS)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/opengl DIR_SRCS)

set(CLIENT_TARGET "live-client")

add_executable(
    ${CLIENT_TARGET}
    ${DIR_SRCS}
)

# 链接库和自定义命令
target_link_libraries(
    ${CLIENT_TARGET}
    pthread m dl z
    LiveRtspClient liveMedia BasicUsageEnvironment groupsock UsageEnvironment 
    ssl crypto
    avformat avcodec avutil swresample 
    X11 
    GL GLEW glut glfw 
)

add_custom_command(
    TARGET ${CLIENT_TARGET} POST_BUILD
    COMMAND ${CROSS}strip ${CLIENT_TARGET}
    COMMAND echo "${CLIENT_TARGET} Generating success..."
)
