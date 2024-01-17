#!/bin/bash
###
 # @Description: 
 # @Version: 
 # @Autor: 李石
 # @Date: 2023-10-19 11:53:05
 # @LastEditors: 李石
 # @LastEditTime: 2024-01-13 11:29:33
### 
# find ../live-test/build ! -name 'build.sh' -mindepth 1 -delete
echo $PWD
if [ "$1" == "all" ]; then
    echo "执行完整脚本..."
    cd ../
    make clean
    make -j$(nproc)
    make install
    cd ./live-test
    rm ./3rdparty/liveRtsplib/* -rf
    cp ../install/include/ 3rdparty/liveRtsplib/ -rf
    cp ../install/lib/ 3rdparty/liveRtsplib/ -rf
    rm build -rf
    mkdir -p build
    cd build
    cmake ..
    cd ..
elif [ "$1" == "live" ]; then
    echo "执行编译live..."
    cd ../
    make -j8
    rm install -rf
    make install
    cd ./live-test
    rm ./3rdparty/liveRtsplib/* -rf
    cp ../install/include/ 3rdparty/liveRtsplib/ -rf
    cp ../install/lib/ 3rdparty/liveRtsplib/ -rf
    rm build -rf
    mkdir -p build
    cd build
    cmake ..
    cd ..
elif [ "$1" == "test" ]; then
    echo "cmake test..."
    rm build -rf
    mkdir -p build
    cd build
    cmake ..
    cd ..
fi
echo "执行编译部分..."
cd build
make -j8
