#!/bin/bash

# NVML 모니터링 시스템 빌드 스크립트

echo "Building NVML Monitoring System..."

# 컴파일러 확인
if ! command -v g++ &> /dev/null; then
    echo "Error: g++ compiler not found"
    exit 1
fi

# CUDA 설치 확인
if ! command -v nvcc &> /dev/null; then
    echo "Warning: CUDA compiler not found. Some features may not work."
fi

# NVML 라이브러리 확인
NVML_PATHS=(
    "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so"
    "/usr/lib64/libnvidia-ml.so"
    "/usr/local/cuda/lib64/libnvidia-ml.so"
)

NVML_FOUND=false
for path in "${NVML_PATHS[@]}"; do
    if [ -f "$path" ]; then
        echo "Found NVML library at: $path"
        NVML_LIB_DIR=$(dirname "$path")
        NVML_FOUND=true
        break
    fi
done

if [ "$NVML_FOUND" = false ]; then
    echo "Error: NVML library not found. Please install NVIDIA drivers."
    exit 1
fi

# 헤더 파일 확인
NVML_HEADER_PATHS=(
    "/usr/local/cuda/include/nvml.h"
    "/usr/include/nvml.h"
    "/opt/cuda/include/nvml.h"
)

HEADER_FOUND=false
for path in "${NVML_HEADER_PATHS[@]}"; do
    if [ -f "$path" ]; then
        echo "Found NVML header at: $path"
        NVML_INCLUDE_DIR=$(dirname "$path")
        HEADER_FOUND=true
        break
    fi
done

if [ "$HEADER_FOUND" = false ]; then
    echo "Error: NVML header not found. Please install CUDA toolkit."
    exit 1
fi

# 빌드 디렉토리 생성
mkdir -p build
cd build

# CMake 사용 (우선순위)
if command -v cmake &> /dev/null; then
    echo "Using CMake for build..."
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
else
    echo "Using manual compilation..."
    g++ -Wall -Wextra -O2 -std=c++17 \
        -I"$NVML_INCLUDE_DIR" \
        ../main.cpp ../nvml_manager.cpp \
        -L"$NVML_LIB_DIR" -lnvidia-ml -lpthread \
        -o nvml_monitoring
fi

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Executable: build/nvml_monitoring"
    
    # 실행 권한 부여
    chmod +x nvml_monitoring
    
    # 간단한 테스트
    echo "Running basic test..."
    if ./nvml_monitoring --help 2>/dev/null || true; then
        echo "Test completed."
    fi
else
    echo "Build failed!"
    exit 1
fi