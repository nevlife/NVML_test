#include "nvml_manager.h"
#include <iostream>
#include <cstring>
#include <sstream>
#include <iomanip>

NVMLManager::NVMLManager() 
    : running(false), initialized(false), eventSet(nullptr) {
}

NVMLManager::~NVMLManager() {
    shutdown();
}

bool NVMLManager::initialize() {
    nvmlReturn_t result = nvmlInit();
    if (result != NVML_SUCCESS) {
        std::cerr << "Failed to initialize NVML: " << nvmlErrorString(result) << std::endl;
        return false;
    }
    
    if (!initializeDevices()) {
        std::cerr << "Failed to initialize devices" << std::endl;
        nvmlShutdown();
        return false;
    }
    
    if (!initializeUnits()) {
        std::cerr << "Warning: Failed to initialize units (S-class only)" << std::endl;
    }
    
    if (!initializeEvents()) {
        std::cerr << "Warning: Failed to initialize events" << std::endl;
    }
    
    initialized = true;
    return true;
}

void NVMLManager::shutdown() {
    if (!initialized) return;
    
    stopMonitoring();
    
    if (eventSet) {
        nvmlEventSetFree(eventSet);
        eventSet = nullptr;
    }
    
    nvmlShutdown();
    initialized = false;
}

bool NVMLManager::initializeDevices() {
    unsigned int deviceCount;
    nvmlReturn_t result = nvmlDeviceGetCount(&deviceCount);
    if (result != NVML_SUCCESS) {
        return false;
    }
    
    gpuDevices.clear();
    gpuDevices.reserve(deviceCount);
    
    for (unsigned int i = 0; i < deviceCount; i++) {
        GPUInfo gpu;
        gpu.index = i;
        
        result = nvmlDeviceGetHandleByIndex(i, &gpu.device);
        if (result != NVML_SUCCESS) continue;
        
        // 디바이스 이름
        char name[NVML_DEVICE_NAME_BUFFER_SIZE];
        if (nvmlDeviceGetName(gpu.device, name, sizeof(name)) == NVML_SUCCESS) {
            gpu.name = name;
        }
        
        // UUID
        char uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
        if (nvmlDeviceGetUUID(gpu.device, uuid, sizeof(uuid)) == NVML_SUCCESS) {
            gpu.uuid = uuid;
        }
        
        // 시리얼 번호
        char serial[NVML_DEVICE_SERIAL_BUFFER_SIZE];
        if (nvmlDeviceGetSerial(gpu.device, serial, sizeof(serial)) == NVML_SUCCESS) {
            gpu.serial = serial;
        }
        
        // PCI 정보
        nvmlPciInfo_t pciInfo;
        if (nvmlDeviceGetPciInfo(gpu.device, &pciInfo) == NVML_SUCCESS) {
            gpu.pciBusId = pciInfo.busId;
        }
        
        // 아키텍처
        nvmlDeviceGetArchitecture(gpu.device, &gpu.architecture);
        
        // CUDA 버전
        nvmlDeviceGetCudaComputeCapability(gpu.device, &gpu.cudaMajor, &gpu.cudaMinor);
        
        // 총 메모리
        nvmlMemory_t memInfo;
        if (nvmlDeviceGetMemoryInfo(gpu.device, &memInfo) == NVML_SUCCESS) {
            gpu.totalMemory = memInfo.total;
        }
        
        gpuDevices.push_back(gpu);
    }
    
    return !gpuDevices.empty();
}

bool NVMLManager::initializeUnits() {
    unsigned int unitCount;
    nvmlReturn_t result = nvmlUnitGetCount(&unitCount);
    if (result != NVML_SUCCESS) {
        return false; // S-class 시스템이 아님
    }
    
    unitDevices.clear();
    unitDevices.reserve(unitCount);
    
    for (unsigned int i = 0; i < unitCount; i++) {
        UnitInfo unit;
        
        result = nvmlUnitGetHandleByIndex(i, &unit.unit);
        if (result != NVML_SUCCESS) continue;
        
        // Unit 정보
        nvmlUnitInfo_t unitInfo;
        if (nvmlUnitGetUnitInfo(unit.unit, &unitInfo) == NVML_SUCCESS) {
            unit.id = unitInfo.id;
            unit.name = unitInfo.name;
            unit.serial = unitInfo.serial;
            unit.firmwareVersion = unitInfo.firmwareVersion;
        }
        
        // 연결된 디바이스들
        unsigned int deviceCount = 0;
        nvmlUnitGetDevices(unit.unit, &deviceCount, nullptr);
        if (deviceCount > 0) {
            unit.devices.resize(deviceCount);
            nvmlUnitGetDevices(unit.unit, &deviceCount, unit.devices.data());
        }
        
        // 팬 속도
        nvmlUnitFanSpeeds_t fanSpeeds;
        if (nvmlUnitGetFanSpeedInfo(unit.unit, &fanSpeeds) == NVML_SUCCESS) {
            unit.fanSpeed = fanSpeeds.fans[0].speed; // 첫 번째 팬
        }
        
        // 온도
        nvmlUnitGetTemperature(unit.unit, 0, &unit.temperature);
        
        // PSU 정보
        nvmlUnitGetPsuInfo(unit.unit, &unit.psuInfo);
        
        unitDevices.push_back(unit);
    }
    
    return true;
}

bool NVMLManager::initializeEvents() {
    nvmlReturn_t result = nvmlEventSetCreate(&eventSet);
    if (result != NVML_SUCCESS) {
        return false;
    }
    
    // 모든 GPU에 대해 기본 이벤트 등록
    for (const auto& gpu : gpuDevices) {
        unsigned long long eventTypes = 
            nvmlEventTypeSingleBitEccError |
            nvmlEventTypeDoubleBitEccError |
            nvmlEventTypePState |
            nvmlEventTypeXidCriticalError;
            
        nvmlDeviceRegisterEvents(gpu.device, eventTypes, eventSet);
    }
    
    return true;
}

GPUMetrics NVMLManager::collectDeviceMetrics(const GPUInfo& gpu) {
    GPUMetrics metrics = {};
    metrics.timestamp = std::chrono::system_clock::now();
    
    // 사용률 정보
    nvmlUtilization_t utilization;
    if (nvmlDeviceGetUtilizationRates(gpu.device, &utilization) == NVML_SUCCESS) {
        metrics.gpuUtilization = utilization.gpu;
        metrics.memoryUtilization = utilization.memory;
    }
    
    // 인코더/디코더 사용률
    unsigned int samplingPeriod;
    nvmlDeviceGetEncoderUtilization(gpu.device, &metrics.encoderUtilization, &samplingPeriod);
    nvmlDeviceGetDecoderUtilization(gpu.device, &metrics.decoderUtilization, &samplingPeriod);
    
    // 메모리 정보
    nvmlMemory_t memInfo;
    if (nvmlDeviceGetMemoryInfo(gpu.device, &memInfo) == NVML_SUCCESS) {
        metrics.memoryUsed = memInfo.used;
        metrics.memoryFree = memInfo.free;
        metrics.memoryTotal = memInfo.total;
    }
    
    // 온도
    nvmlDeviceGetTemperature(gpu.device, NVML_TEMPERATURE_GPU, &metrics.temperature);
    
    // 팬 속도
    nvmlDeviceGetFanSpeed(gpu.device, &metrics.fanSpeed);
    
    // 전력 정보
    nvmlDeviceGetPowerUsage(gpu.device, &metrics.powerUsage);
    nvmlDeviceGetPowerManagementLimitConstraints(gpu.device, nullptr, &metrics.powerLimit);
    nvmlDeviceGetPerformanceState(gpu.device, &metrics.powerState);
    
    // 클럭 정보
    nvmlDeviceGetClockInfo(gpu.device, NVML_CLOCK_GRAPHICS, &metrics.graphicsClock);
    nvmlDeviceGetClockInfo(gpu.device, NVML_CLOCK_MEM, &metrics.memoryClock);
    nvmlDeviceGetClockInfo(gpu.device, NVML_CLOCK_SM, &metrics.smClock);
    
    // ECC 에러
    nvmlEccErrorCounts_t eccCounts;
    if (nvmlDeviceGetTotalEccErrors(gpu.device, NVML_SINGLE_BIT_ECC, NVML_VOLATILE_ECC, &eccCounts) == NVML_SUCCESS) {
        metrics.eccSingleBit = eccCounts.deviceMemory;
    }
    if (nvmlDeviceGetTotalEccErrors(gpu.device, NVML_DOUBLE_BIT_ECC, NVML_VOLATILE_ECC, &eccCounts) == NVML_SUCCESS) {
        metrics.eccDoubleBit = eccCounts.deviceMemory;
    }
    
    return metrics;
}

std::vector<ProcessInfo> NVMLManager::collectProcessInfo(nvmlDevice_t device) {
    std::vector<ProcessInfo> processes;
    
    // Compute 프로세스
    unsigned int infoCount = 0;
    nvmlDeviceGetComputeRunningProcesses(device, &infoCount, nullptr);
    if (infoCount > 0) {
        std::vector<nvmlProcessInfo_t> nvmlProcs(infoCount);
        if (nvmlDeviceGetComputeRunningProcesses(device, &infoCount, nvmlProcs.data()) == NVML_SUCCESS) {
            for (const auto& proc : nvmlProcs) {
                ProcessInfo info;
                info.pid = proc.pid;
                info.usedGpuMemory = proc.usedGpuMemory;
                info.type = NVML_PROCESS_TYPE_COMPUTE;
                
                // 프로세스 이름 가져오기 (시스템 의존적)
                char processName[1024];
                if (nvmlSystemGetProcessName(proc.pid, processName, sizeof(processName)) == NVML_SUCCESS) {
                    info.name = processName;
                }
                
                processes.push_back(info);
            }
        }
    }
    
    // Graphics 프로세스
    infoCount = 0;
    nvmlDeviceGetGraphicsRunningProcesses(device, &infoCount, nullptr);
    if (infoCount > 0) {
        std::vector<nvmlProcessInfo_t> nvmlProcs(infoCount);
        if (nvmlDeviceGetGraphicsRunningProcesses(device, &infoCount, nvmlProcs.data()) == NVML_SUCCESS) {
            for (const auto& proc : nvmlProcs) {
                ProcessInfo info;
                info.pid = proc.pid;
                info.usedGpuMemory = proc.usedGpuMemory;
                info.type = NVML_PROCESS_TYPE_GRAPHICS;
                
                char processName[1024];
                if (nvmlSystemGetProcessName(proc.pid, processName, sizeof(processName)) == NVML_SUCCESS) {
                    info.name = processName;
                }
                
                processes.push_back(info);
            }
        }
    }
    
    return processes;
}

void NVMLManager::startMonitoring() {
    if (running || !initialized) return;
    
    running = true;
    monitorThread = std::thread(&NVMLManager::monitoringLoop, this);
    
    if (enableEventMonitoring && eventSet) {
        eventThread = std::thread(&NVMLManager::eventLoop, this);
    }
}

void NVMLManager::stopMonitoring() {
    if (!running) return;
    
    running = false;
    eventCV.notify_all();
    
    if (monitorThread.joinable()) {
        monitorThread.join();
    }
    if (eventThread.joinable()) {
        eventThread.join();
    }
}

void NVMLManager::monitoringLoop() {
    while (running) {
        auto start = std::chrono::steady_clock::now();
        
        // 모든 GPU 메트릭 수집
        for (const auto& gpu : gpuDevices) {
            GPUMetrics metrics = collectDeviceMetrics(gpu);
            if (metricsCallback) {
                metricsCallback(metrics);
            }
            
            // 프로세스 정보 수집
            if (enableProcessMonitoring && processCallback) {
                auto processes = collectProcessInfo(gpu.device);
                if (!processes.empty()) {
                    processCallback(processes);
                }
            }
        }
        
        // 모니터링 간격 대기
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto sleepTime = std::chrono::milliseconds(monitoringInterval) - elapsed;
        
        if (sleepTime.count() > 0) {
            std::this_thread::sleep_for(sleepTime);
        }
    }
}

void NVMLManager::eventLoop() {
    while (running) {
        nvmlEventData_t eventData;
        nvmlReturn_t result = nvmlEventSetWait(eventSet, &eventData, 1000); // 1초 타임아웃
        
        if (result == NVML_SUCCESS) {
            handleEvent(eventData);
        } else if (result != NVML_ERROR_TIMEOUT) {
            std::cerr << "Event wait error: " << nvmlErrorString(result) << std::endl;
        }
    }
}

void NVMLManager::handleEvent(const nvmlEventData_t& eventData) {
    EventInfo event;
    event.device = eventData.device;
    event.eventType = eventData.eventType;
    event.timestamp = std::chrono::system_clock::now();
    event.description = eventTypeToString(eventData.eventType);
    
    if (eventCallback) {
        eventCallback(event);
    }
}

std::string NVMLManager::eventTypeToString(unsigned long long eventType) {
    switch (eventType) {
        case nvmlEventTypeSingleBitEccError: return "Single Bit ECC Error";
        case nvmlEventTypeDoubleBitEccError: return "Double Bit ECC Error";
        case nvmlEventTypePState: return "Performance State Change";
        case nvmlEventTypeXidCriticalError: return "Xid Critical Error";
        default: return "Unknown Event";
    }
}

GPUMetrics NVMLManager::getGPUMetrics(unsigned int deviceIndex) {
    if (deviceIndex >= gpuDevices.size()) {
        return {};
    }
    return collectDeviceMetrics(gpuDevices[deviceIndex]);
}

std::vector<GPUMetrics> NVMLManager::getAllGPUMetrics() {
    std::vector<GPUMetrics> allMetrics;
    for (const auto& gpu : gpuDevices) {
        allMetrics.push_back(collectDeviceMetrics(gpu));
    }
    return allMetrics;
}

std::vector<ProcessInfo> NVMLManager::getRunningProcesses(unsigned int deviceIndex) {
    if (deviceIndex >= gpuDevices.size()) {
        return {};
    }
    return collectProcessInfo(gpuDevices[deviceIndex].device);
}

BAR1MemoryInfo NVMLManager::getBAR1MemoryInfo(unsigned int deviceIndex) {
    BAR1MemoryInfo info = {};
    if (deviceIndex >= gpuDevices.size()) {
        return info;
    }
    
    nvmlBAR1Memory_t bar1Memory;
    if (nvmlDeviceGetBAR1MemoryInfo(gpuDevices[deviceIndex].device, &bar1Memory) == NVML_SUCCESS) {
        info.bar1Total = bar1Memory.bar1Total;
        info.bar1Used = bar1Memory.bar1Used;
        info.bar1Free = bar1Memory.bar1Free;
    }
    
    return info;
}

std::string NVMLManager::getDriverVersion() {
    char version[NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE];
    if (nvmlSystemGetDriverVersion(version, sizeof(version)) == NVML_SUCCESS) {
        return version;
    }
    return "Unknown";
}

std::string NVMLManager::getNVMLVersion() {
    char version[NVML_SYSTEM_NVML_VERSION_BUFFER_SIZE];
    if (nvmlSystemGetNVMLVersion(version, sizeof(version)) == NVML_SUCCESS) {
        return version;
    }
    return "Unknown";
}

std::string NVMLManager::getCUDAVersion() {
    int cudaDriverVersion;
    if (nvmlSystemGetCudaDriverVersion(&cudaDriverVersion) == NVML_SUCCESS) {
        int major = cudaDriverVersion / 1000;
        int minor = (cudaDriverVersion % 1000) / 10;
        return std::to_string(major) + "." + std::to_string(minor);
    }
    return "Unknown";
}

std::string NVMLManager::getErrorString(nvmlReturn_t result) {
    return nvmlErrorString(result);
}

bool NVMLManager::isDeviceValid(unsigned int deviceIndex) {
    return deviceIndex < gpuDevices.size();
}

std::vector<GPUInfo> NVMLManager::getGPUInfo() {
    return gpuDevices;
}

std::vector<UnitInfo> NVMLManager::getUnitInfo() {
    return unitDevices;
}

void NVMLManager::setMetricsCallback(std::function<void(const GPUMetrics&)> callback) {
    metricsCallback = callback;
}

void NVMLManager::setEventCallback(std::function<void(const EventInfo&)> callback) {
    eventCallback = callback;
}

void NVMLManager::setProcessCallback(std::function<void(const std::vector<ProcessInfo>&)> callback) {
    processCallback = callback;
}

void NVMLManager::setMonitoringInterval(int intervalMs) {
    monitoringInterval = intervalMs;
}