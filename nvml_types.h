#ifndef NVML_TYPES_H
#define NVML_TYPES_H

#include <nvml.h>
#include <vector>
#include <string>
#include <chrono>
#include <memory>

// GPU 기본 정보 구조체
struct GPUInfo {
    nvmlDevice_t device;
    std::string name;
    std::string uuid;
    std::string serial;
    std::string pciBusId;
    unsigned int index;
    nvmlDeviceArchitecture_t architecture;
    int cudaMajor;
    int cudaMinor;
    unsigned long long totalMemory;
};

// GPU 성능 메트릭 구조체
struct GPUMetrics {
    // 사용률
    unsigned int gpuUtilization;
    unsigned int memoryUtilization;
    unsigned int encoderUtilization;
    unsigned int decoderUtilization;
    
    // 메모리 정보
    unsigned long long memoryUsed;
    unsigned long long memoryFree;
    unsigned long long memoryTotal;
    
    // 온도 및 전력
    unsigned int temperature;
    unsigned int fanSpeed;
    unsigned int powerUsage;
    unsigned int powerLimit;
    nvmlPstates_t powerState;
    
    // 클럭 정보
    unsigned int graphicsClock;
    unsigned int memoryClock;
    unsigned int smClock;
    
    // 에러 카운트
    unsigned long long eccSingleBit;
    unsigned long long eccDoubleBit;
    
    // 타임스탬프
    std::chrono::system_clock::time_point timestamp;
};

// 프로세스 정보 구조체
struct ProcessInfo {
    unsigned int pid;
    std::string name;
    unsigned long long usedGpuMemory;
    nvmlProcessType_t type;  // Graphics or Compute
};

// 이벤트 정보 구조체
struct EventInfo {
    nvmlDevice_t device;
    unsigned long long eventType;
    std::chrono::system_clock::time_point timestamp;
    std::string description;
};

// Unit 정보 구조체 (S-class systems)
struct UnitInfo {
    nvmlUnit_t unit;
    std::string id;
    std::string name;
    std::string serial;
    std::string firmwareVersion;
    std::vector<nvmlDevice_t> devices;
    unsigned int fanSpeed;
    unsigned int temperature;
    nvmlPSUInfo_t psuInfo;
};

// BAR1 메모리 정보
struct BAR1MemoryInfo {
    unsigned long long bar1Total;
    unsigned long long bar1Used;
    unsigned long long bar1Free;
};

// vGPU 정보 구조체
struct VGPUInfo {
    unsigned int vgpuInstance;
    std::string vgpuType;
    unsigned long long framebufferSize;
    unsigned int maxInstances;
    unsigned int createdInstances;
};

#endif // NVML_TYPES_H