#!/bin/bash

# NVML 단순 컴파일 스크립트

echo "컴파일을 시작합니다..."

# NVML 경로 설정
NVML_INCLUDE_DIR="/usr/local/cuda-12.6/targets/x86_64-linux/include"
NVML_LIB_DIR="/usr/local/cuda-12.6/targets/x86_64-linux/lib/stubs"

# 빌드 디렉토리 생성
mkdir -p build

# 기본 테스트 프로그램 컴파일
echo "기본 NVML 테스트 프로그램 컴파일 중..."
g++ -Wall -O2 -std=c++17 \
    -I"$NVML_INCLUDE_DIR" \
    simple_nvml_test.cpp \
    -L"$NVML_LIB_DIR" -lnvidia-ml -lpthread \
    -o build/simple_nvml_test

if [ $? -eq 0 ]; then
    echo "simple_nvml_test 컴파일 성공!"
else
    echo "simple_nvml_test 컴파일 실패!"
fi

# GPU 정보 프로그램 컴파일
echo "GPU 정보 프로그램 컴파일 중..."
g++ -Wall -O2 -std=c++17 \
    -I"$NVML_INCLUDE_DIR" \
    gpu_info.cpp \
    -L"$NVML_LIB_DIR" -lnvidia-ml -lpthread \
    -o build/gpu_info

if [ $? -eq 0 ]; then
    echo "gpu_info 컴파일 성공!"
else
    echo "gpu_info 컴파일 실패!"
fi

# GPU 모니터링 프로그램 컴파일
echo "GPU 모니터링 프로그램 컴파일 중..."
g++ -Wall -O2 -std=c++17 \
    -I"$NVML_INCLUDE_DIR" \
    gpu_monitor.cpp \
    -L"$NVML_LIB_DIR" -lnvidia-ml -lpthread \
    -o build/gpu_monitor

if [ $? -eq 0 ]; then
    echo "gpu_monitor 컴파일 성공!"
else
    echo "gpu_monitor 컴파일 실패!"
fi

echo "컴파일 작업이 완료되었습니다." 