cmake_minimum_required(VERSION 3.10)
project(NVMLMonitoring LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CUDA 설정
find_package(CUDA REQUIRED)
enable_language(CUDA)

# NVML 라이브러리 찾기
find_library(NVML_LIBRARY nvidia-ml
    HINTS
    /usr/lib/x86_64-linux-gnu
    /usr/lib64
    /usr/local/cuda/lib64
    /usr/local/cuda/lib
    /usr/local/cuda-12.6/targets/x86_64-linux/lib
    /usr/local/cuda-12.6/targets/x86_64-linux/lib/stubs
    $ENV{CUDA_PATH}/lib/x64
    "C:/Program Files/NVIDIA Corporation/NVSMI"
)

if(NOT NVML_LIBRARY)
    message(FATAL_ERROR "NVML library not found")
endif()

# 헤더 파일 경로
include_directories(
    ${CUDA_INCLUDE_DIRS}
    /usr/local/cuda/include
    /usr/local/cuda-12.6/targets/x86_64-linux/include
    $ENV{CUDA_PATH}/include
    "C:/Program Files/NVIDIA Corporation/NVSMI"
)

# 소스 파일
set(SOURCES
    main.cpp
    nvml_manager.cpp
    nvml_field_queries.cpp
    nvml_accounting.cpp
    nvml_mig.cpp
)

# 헤더 파일
set(HEADERS
    nvml_types.h
    nvml_manager.h
)

# 실행 파일 생성
add_executable(nvml_monitoring ${SOURCES} ${HEADERS})

# 라이브러리 링크
target_link_libraries(nvml_monitoring 
    ${NVML_LIBRARY}
    ${CUDA_LIBRARIES}
    pthread
)

# 컴파일 옵션
target_compile_options(nvml_monitoring PRIVATE
    -Wall
    -Wextra
    -O2
)

# 설치 규칙
install(TARGETS nvml_monitoring DESTINATION bin)