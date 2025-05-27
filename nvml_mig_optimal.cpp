#include "nvml_mig_optimal.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>

namespace nvml_mig {

// 정적 멤버 초기화
std::unique_ptr<MIGManager> MIGManager::instance = nullptr;
std::mutex MIGManager::instanceMutex;

// 싱글톤 인스턴스 접근
MIGManager& MIGManager::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (!instance) {
        instance = std::unique_ptr<MIGManager>(new MIGManager());
    }
    return *instance;
}

// 생성자
MIGManager::MIGManager() 
    : monitoringActive(false), workerActive(false) {
    try {
        // NVML 초기화
        nvmlGuard = std::make_unique<NVMLUtil::NVMLGuard>();
        
        // 디바이스 초기화
        initializeDevices();
        
        // 작업자 스레드 시작
        workerActive = true;
        workerThread = std::thread(&MIGManager::workerLoop, this);
    }
    catch (const NVMLException& e) {
        std::cerr << "MIGManager 초기화 실패: " << e.what() << std::endl;
        throw;
    }
}

// 소멸자
MIGManager::~MIGManager() {
    // 모니터링 중지
    stopMonitoring();
    
    // 작업자 스레드 중지
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        workerActive = false;
    }
    taskCV.notify_one();
    
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

// GPU 디바이스 초기화
void MIGManager::initializeDevices() {
    unsigned int deviceCount = 0;
    nvmlReturn_t result = nvmlDeviceGetCount(&deviceCount);
    if (result != NVML_SUCCESS) {
        throw NVMLException(result, "디바이스 개수 조회 실패");
    }
    
    devices.clear();
    for (unsigned int i = 0; i < deviceCount; i++) {
        nvmlDevice_t device;
        result = nvmlDeviceGetHandleByIndex(i, &device);
        if (result == NVML_SUCCESS) {
            devices.push_back(device);
        }
        else {
            std::cerr << "경고: 디바이스 " << i << " 핸들 획득 실패" << std::endl;
        }
    }
    
    // 모든 MIG 디바이스 정보 수집
    refreshMIGDevices();
}

// MIG 디바이스 정보 새로고침
void MIGManager::refreshMIGDevices() {
    std::map<std::string, MIGDeviceInfo> newDevices;
    
    for (size_t deviceIndex = 0; deviceIndex < devices.size(); deviceIndex++) {
        if (!isMIGModeEnabled(deviceIndex)) {
            continue;
        }
        
        unsigned int count = 0;
        nvmlGpuInstance_t gpuInstances[NVML_MAX_GPU_INSTANCES];
        
        nvmlReturn_t result = nvmlDeviceGetGpuInstances(devices[deviceIndex], 
                                                      NVML_MAX_GPU_INSTANCES,
                                                      gpuInstances, &count);
        
        if (result == NVML_SUCCESS) {
            for (unsigned int i = 0; i < count; i++) {
                MIGDeviceInfo migInfo;
                migInfo.parentDeviceIndex = deviceIndex;
                
                nvmlGpuInstanceInfo_t instanceInfo;
                if (nvmlGpuInstanceGetInfo(gpuInstances[i], &instanceInfo) == NVML_SUCCESS) {
                    migInfo.instanceId = instanceInfo.id;
                    migInfo.profileId = instanceInfo.profileId;
                    
                    // 컴퓨트 인스턴스 정보 수집
                    unsigned int ciCount = 0;
                    nvmlComputeInstance_t computeInstances[NVML_MAX_COMPUTE_INSTANCES];
                    
                    if (nvmlGpuInstanceGetComputeInstances(gpuInstances[i], 
                                                         NVML_MAX_COMPUTE_INSTANCES,
                                                         computeInstances, &ciCount) == NVML_SUCCESS) {
                        
                        migInfo.computeInstanceIds.clear();
                        migInfo.currentComputeInstances = ciCount;
                        
                        for (unsigned int j = 0; j < ciCount; j++) {
                            nvmlComputeInstanceInfo_t ciInfo;
                            if (nvmlComputeInstanceGetInfo(computeInstances[j], &ciInfo) == NVML_SUCCESS) {
                                migInfo.computeInstanceIds.push_back(ciInfo.id);
                                
                                // 첫 번째 컴퓨트 인스턴스에서 디바이스 핸들 가져오기
                                if (j == 0) {
                                    // NVML 버전에 따라 다른 함수 사용
                                    #if defined(NVML_API_VERSION) && NVML_API_VERSION >= 11000
                                    nvmlDeviceGetComputeInstanceDeviceHandleByIndex(devices[deviceIndex], 
                                                                                 instanceInfo.id, 
                                                                                 ciInfo.id, 
                                                                                 &migInfo.deviceHandle);
                                    #else
                                    nvmlComputeInstanceGetDeviceHandle(computeInstances[j], &migInfo.deviceHandle);
                                    #endif
                                    
                                    // UUID 가져오기
                                    char uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
                                    if (nvmlDeviceGetUUID(migInfo.deviceHandle, uuid, NVML_DEVICE_UUID_BUFFER_SIZE) == NVML_SUCCESS) {
                                        migInfo.uuid = uuid;
                                    }
                                    
                                    // 메모리 정보
                                    nvmlMemory_t memInfo;
                                    if (nvmlDeviceGetMemoryInfo(migInfo.deviceHandle, &memInfo) == NVML_SUCCESS) {
                                        migInfo.memorySize = memInfo.total;
                                    }
                                    
                                    // 프로파일 정보에서 최대 컴퓨트 인스턴스 수 가져오기
                                    nvmlGpuInstanceProfileInfo_t profileInfo;
                                    if (nvmlDeviceGetGpuInstanceProfileInfo(devices[deviceIndex], 
                                                                          instanceInfo.profileId, 
                                                                          &profileInfo) == NVML_SUCCESS) {
                                        migInfo.maxComputeInstances = profileInfo.maxComputeInstances;
                                        migInfo.multiprocessorCount = profileInfo.multiprocessorCount;
                                    }
                                }
                            }
                        }
                        
                        // UUID를 키로 사용하여 맵에 추가
                        if (!migInfo.uuid.empty()) {
                            newDevices[migInfo.uuid] = migInfo;
                        }
                    }
                }
            }
        }
    }
    
    // 스레드 안전하게 맵 교체
    {
        std::lock_guard<std::mutex> lock(metricsMutex);
        migDevices = std::move(newDevices);
    }
}

// MIG 모드 활성화
bool MIGManager::enableMIGMode(unsigned int deviceIndex, bool async, 
                             std::function<void(bool, const std::string&)> callback) {
    // 디바이스 인덱스 검사
    if (deviceIndex >= devices.size()) {
        if (callback) callback(false, "유효하지 않은 디바이스 인덱스");
        return false;
    }
    
    // 비동기 모드
    if (async) {
        std::function<void()> task = [this, deviceIndex]() {
            nvmlReturn_t result = nvmlDeviceSetMigMode(devices[deviceIndex], NVML_ENABLE_MIG);
            if (result != NVML_SUCCESS) {
                throw NVMLException(result, "MIG 모드 활성화 실패");
            }
            refreshMIGDevices();
        };
        
        // 작업 큐에 추가
        {
            std::lock_guard<std::mutex> lock(taskMutex);
            taskQueue.push({task, callback});
        }
        taskCV.notify_one();
        return true;
    }
    // 동기 모드
    else {
        try {
            nvmlReturn_t result = nvmlDeviceSetMigMode(devices[deviceIndex], NVML_ENABLE_MIG);
            if (result != NVML_SUCCESS) {
                if (callback) callback(false, nvmlErrorString(result));
                return false;
            }
            
            refreshMIGDevices();
            if (callback) callback(true, "MIG 모드 활성화 성공");
            return true;
        }
        catch (const std::exception& e) {
            if (callback) callback(false, e.what());
            return false;
        }
    }
}

// MIG 모드 비활성화
bool MIGManager::disableMIGMode(unsigned int deviceIndex, bool async, 
                              std::function<void(bool, const std::string&)> callback) {
    // 디바이스 인덱스 검사
    if (deviceIndex >= devices.size()) {
        if (callback) callback(false, "유효하지 않은 디바이스 인덱스");
        return false;
    }
    
    // 비동기 모드
    if (async) {
        std::function<void()> task = [this, deviceIndex]() {
            nvmlReturn_t result = nvmlDeviceSetMigMode(devices[deviceIndex], NVML_DISABLE_MIG);
            if (result != NVML_SUCCESS) {
                throw NVMLException(result, "MIG 모드 비활성화 실패");
            }
            refreshMIGDevices();
        };
        
        // 작업 큐에 추가
        {
            std::lock_guard<std::mutex> lock(taskMutex);
            taskQueue.push({task, callback});
        }
        taskCV.notify_one();
        return true;
    }
    // 동기 모드
    else {
        try {
            nvmlReturn_t result = nvmlDeviceSetMigMode(devices[deviceIndex], NVML_DISABLE_MIG);
            if (result != NVML_SUCCESS) {
                if (callback) callback(false, nvmlErrorString(result));
                return false;
            }
            
            refreshMIGDevices();
            if (callback) callback(true, "MIG 모드 비활성화 성공");
            return true;
        }
        catch (const std::exception& e) {
            if (callback) callback(false, e.what());
            return false;
        }
    }
}

// MIG 모드 상태 확인
bool MIGManager::isMIGModeEnabled(unsigned int deviceIndex) {
    if (deviceIndex >= devices.size()) {
        return false;
    }
    
    unsigned int currentMode, pendingMode;
    nvmlReturn_t result = nvmlDeviceGetMigMode(devices[deviceIndex], &currentMode, &pendingMode);
    
    return (result == NVML_SUCCESS && currentMode == NVML_ENABLE_MIG);
}

// GPU 인스턴스 프로파일 조회
std::vector<MIGProfile> MIGManager::getAvailableProfiles(unsigned int deviceIndex) {
    std::vector<MIGProfile> profiles;
    
    if (deviceIndex >= devices.size()) {
        return profiles;
    }
    
    // 모든 사용 가능한 프로파일 조회
    for (unsigned int profileId = 0; profileId < 8; profileId++) {
        nvmlGpuInstanceProfileInfo_t profileInfo;
        nvmlReturn_t result = nvmlDeviceGetGpuInstanceProfileInfo(devices[deviceIndex], profileId, &profileInfo);
        
        if (result == NVML_SUCCESS) {
            MIGProfile profile;
            profile.profileId = profileId;
            profile.memorySizeMB = profileInfo.memorySizeMB;
            profile.multiprocessorCount = profileInfo.multiprocessorCount;
            profile.maxComputeInstances = profileInfo.maxComputeInstances;
            
            // 디바이스 이름 가져오기
            char name[NVML_DEVICE_NAME_BUFFER_SIZE];
            if (nvmlDeviceGetName(devices[deviceIndex], name, NVML_DEVICE_NAME_BUFFER_SIZE) == NVML_SUCCESS) {
                profile.name = std::string(name) + "_Profile_" + std::to_string(profileId);
            } else {
                profile.name = "GPU" + std::to_string(deviceIndex) + "_Profile_" + std::to_string(profileId);
            }
            
            profiles.push_back(profile);
        }
    }
    
    return profiles;
}

// 작업자 스레드 함수
void MIGManager::workerLoop() {
    while (workerActive) {
        AsyncTask task;
        
        // 작업 대기
        {
            std::unique_lock<std::mutex> lock(taskMutex);
            taskCV.wait(lock, [this] {
                return !workerActive || !taskQueue.empty();
            });
            
            if (!workerActive) {
                break;
            }
            
            if (!taskQueue.empty()) {
                task = taskQueue.front();
                taskQueue.pop();
            }
        }
        
        // 작업 실행
        if (task.task) {
            try {
                task.task();
                if (task.callback) {
                    task.callback(true, "작업 성공");
                }
            }
            catch (const std::exception& e) {
                if (task.callback) {
                    task.callback(false, e.what());
                }
            }
        }
    }
}

// 모니터링 스레드 함수
void MIGManager::monitoringLoop() {
    while (monitoringActive) {
        // MIG 디바이스 정보 새로고침
        refreshMIGDevices();
        
        // 모든 MIG 디바이스의 메트릭 수집
        std::map<std::string, MIGMetrics> metrics;
        
        {
            std::lock_guard<std::mutex> lock(metricsMutex);
            for (const auto& [uuid, device] : migDevices) {
                metrics[uuid] = collectDeviceMetrics(device);
            }
        }
        
        // 최신 메트릭 업데이트
        {
            std::lock_guard<std::mutex> lock(metricsMutex);
            latestMetrics = std::move(metrics);
        }
        
        // 주기적으로 대기
        {
            std::unique_lock<std::mutex> lock(metricsMutex);
            monitoringCV.wait_for(lock, std::chrono::milliseconds(1000), [this] {
                return !monitoringActive;
            });
        }
    }
}

// MIG 디바이스 메트릭 수집
MIGMetrics MIGManager::collectDeviceMetrics(const MIGDeviceInfo& device) {
    MIGMetrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    
    // 사용률 정보
    nvmlUtilization_t utilization;
    if (nvmlDeviceGetUtilizationRates(device.deviceHandle, &utilization) == NVML_SUCCESS) {
        metrics.gpuUtilization = utilization.gpu;
        metrics.memoryUtilization = utilization.memory;
    }
    
    // 메모리 정보
    nvmlMemory_t memInfo;
    if (nvmlDeviceGetMemoryInfo(device.deviceHandle, &memInfo) == NVML_SUCCESS) {
        metrics.memoryUsed = memInfo.used;
        metrics.memoryFree = memInfo.free;
        metrics.memoryTotal = memInfo.total;
    }
    
    // 전력 사용량 (부모 디바이스에서)
    unsigned int power = 0;
    if (nvmlDeviceGetPowerUsage(devices[device.parentDeviceIndex], &power) == NVML_SUCCESS) {
        metrics.powerUsage = power;
    }
    
    // 온도 (부모 디바이스에서)
    unsigned int temp = 0;
    if (nvmlDeviceGetTemperature(devices[device.parentDeviceIndex], NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
        metrics.temperature = temp;
    }
    
    // 프로세스 정보
    unsigned int processCount = 0;
    nvmlProcessInfo_t processes[16];
    nvmlReturn_t result = nvmlDeviceGetComputeRunningProcesses(device.deviceHandle, &processCount, processes);
    
    if (result == NVML_SUCCESS && processCount > 0) {
        for (unsigned int i = 0; i < processCount; i++) {
            char name[256] = {0};
            nvmlSystemGetProcessName(processes[i].pid, name, sizeof(name));
            
            std::string processName = name;
            if (processName.empty()) {
                processName = "pid_" + std::to_string(processes[i].pid);
            }
            
            metrics.processUtilization[processName] = processes[i].usedGpuMemory / (1024 * 1024); // MB 단위
        }
    }
    
    return metrics;
}

// 모니터링 시작
void MIGManager::startMonitoring(unsigned int intervalMs) {
    // 이미 모니터링 중이라면 중지
    stopMonitoring();
    
    monitoringActive = true;
    monitoringThread = std::thread(&MIGManager::monitoringLoop, this);
}

// 모니터링 중지
void MIGManager::stopMonitoring() {
    if (monitoringActive) {
        monitoringActive = false;
        monitoringCV.notify_all();
        
        if (monitoringThread.joinable()) {
            monitoringThread.join();
        }
    }
}

// 모든 MIG 디바이스 정보 조회
std::vector<MIGDeviceInfo> MIGManager::getAllMIGDevices() {
    std::vector<MIGDeviceInfo> devices;
    
    // MIG 디바이스 정보 새로고침
    refreshMIGDevices();
    
    // 모든 MIG 디바이스 수집
    {
        std::lock_guard<std::mutex> lock(metricsMutex);
        devices.reserve(migDevices.size());
        for (const auto& [_, device] : migDevices) {
            devices.push_back(device);
        }
    }
    
    return devices;
}

// 특정 디바이스의 MIG 인스턴스 조회
std::vector<MIGDeviceInfo> MIGManager::getMIGDevices(unsigned int deviceIndex) {
    std::vector<MIGDeviceInfo> devices;
    
    if (deviceIndex >= this->devices.size()) {
        return devices;
    }
    
    // MIG 디바이스 정보 새로고침
    refreshMIGDevices();
    
    // 특정 디바이스의 MIG 인스턴스 수집
    {
        std::lock_guard<std::mutex> lock(metricsMutex);
        for (const auto& [_, device] : migDevices) {
            if (device.parentDeviceIndex == deviceIndex) {
                devices.push_back(device);
            }
        }
    }
    
    return devices;
}

// UUID로 MIG 디바이스 찾기
std::optional<MIGDeviceInfo> MIGManager::findMIGDeviceByUUID(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(metricsMutex);
    
    auto it = migDevices.find(uuid);
    if (it != migDevices.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

// MIG 디바이스 메트릭 조회
std::optional<MIGMetrics> MIGManager::getMIGDeviceMetrics(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(metricsMutex);
    
    auto it = latestMetrics.find(uuid);
    if (it != latestMetrics.end()) {
        return it->second;
    }
    
    // 메트릭이 없으면 디바이스 찾아서 수집
    auto deviceIt = migDevices.find(uuid);
    if (deviceIt != migDevices.end()) {
        return collectDeviceMetrics(deviceIt->second);
    }
    
    return std::nullopt;
}

// 모든 MIG 디바이스 메트릭 조회
std::map<std::string, MIGMetrics> MIGManager::getAllMIGMetrics() {
    std::map<std::string, MIGMetrics> metrics;
    
    {
        std::lock_guard<std::mutex> lock(metricsMutex);
        
        // 캐시된 메트릭이 없으면 새로 수집
        if (latestMetrics.empty()) {
            for (const auto& [uuid, device] : migDevices) {
                metrics[uuid] = collectDeviceMetrics(device);
            }
        } else {
            metrics = latestMetrics;
        }
    }
    
    return metrics;
}

// 장치 개수 조회
size_t MIGManager::getDeviceCount() const {
    return devices.size();
}

// 장치 핸들 조회
nvmlDevice_t MIGManager::getDeviceHandle(unsigned int index) const {
    if (index < devices.size()) {
        return devices[index];
    }
    return nullptr;
}

// 장치 이름 조회
std::string MIGManager::getDeviceName(unsigned int index) const {
    if (index >= devices.size()) {
        return "";
    }
    
    char name[NVML_DEVICE_NAME_BUFFER_SIZE];
    nvmlReturn_t result = nvmlDeviceGetName(devices[index], name, NVML_DEVICE_NAME_BUFFER_SIZE);
    
    if (result == NVML_SUCCESS) {
        return name;
    }
    
    return "Unknown";
}

} // namespace nvml_mig 