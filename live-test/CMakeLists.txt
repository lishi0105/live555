cmake_minimum_required(VERSION 2.8)
project(authServer)

set(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -Wall -Wno-unused-function -O0 -g -fPIC -Wno-sign-compare -Wno-unused-value -Wno-unused-variable -fstack-protector-all")
set(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -Wall -Wno-unused-function -O3 -s -fPIC -Wno-sign-compare -Wno-unused-value -Wno-unused-variable -fstack-protector-all")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -Wall -Wno-unused-function -O0 -g -fPIC -Wno-sign-compare -Wno-unused-value -Wno-unused-variable -fstack-protector-all")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -Wall -Wno-unused-function -O3 -s -fPIC -Wno-sign-compare -Wno-unused-value -Wno-unused-variable -fstack-protector-all")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

message("ARCH is ${ARCH}")

SET(FFMPEG_LIB_DIR ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg)
SET(LIVERTSP_LIB_DIR ${CMAKE_SOURCE_DIR}/3rdparty/liveRtsplib)

include_directories(
    ${LIVERTSP_LIB_DIR}/include/groupsock
    ${LIVERTSP_LIB_DIR}/include/liveMedia
    ${LIVERTSP_LIB_DIR}/include/liveRtspClient
    ${LIVERTSP_LIB_DIR}/include/liveRtspServer
    ${LIVERTSP_LIB_DIR}/include/UsageEnvironment
    ${LIVERTSP_LIB_DIR}/include/BasicUsageEnvironment
    ${FFMPEG_LIB_DIR}/include
)

link_directories(
    ${LIVERTSP_LIB_DIR}/lib
    ${FFMPEG_LIB_DIR}/lib
)

add_subdirectory(client)
add_subdirectory(server)
