#include "nvml_types.h"
#include <vector>
#include <map>

struct MIGDeviceInfo {
    nvmlDevice_t migDevice;
    unsigned int instanceId;
    unsigned int computeInstanceId;
    nvmlDeviceAttributes_t attributes;
    std::string uuid;
    unsigned long long memorySize;
    unsigned int multiprocessorCount;
    unsigned int maxComputeInstances;
    unsigned int currentComputeInstances;
};

struct GPUInstanceProfile {
    unsigned int profileId;
    unsigned long long memorySizeMB;
    unsigned int multiprocessorCount;
    unsigned int maxComputeInstances;
    std::string name;
};

class NVMLMIGManager {
private:
    std::vector<nvmlDevice_t> parentDevices;
    std::map<nvmlDevice_t, std::vector<MIGDeviceInfo>> migInstances;
    
public:
    NVMLMIGManager(const std::vector<nvmlDevice_t>& devices) 
        : parentDevices(devices) {}
    
    // MIG 모드 활성화
    bool enableMIGMode(unsigned int deviceIndex) {
        if (deviceIndex >= parentDevices.size()) return false;
        
        nvmlReturn_t result = nvmlDeviceSetMigMode(parentDevices[deviceIndex], 1);
        return result == NVML_SUCCESS;
    }
    
    // MIG 모드 비활성화
    bool disableMIGMode(unsigned int deviceIndex) {
        if (deviceIndex >= parentDevices.size()) return false;
        
        nvmlReturn_t result = nvmlDeviceSetMigMode(parentDevices[deviceIndex], 0);
        return result == NVML_SUCCESS;
    }
    
    // MIG 모드 상태 확인
    bool isMIGModeEnabled(unsigned int deviceIndex) {
        if (deviceIndex >= parentDevices.size()) return false;
        
        unsigned int currentMode, pendingMode;
        nvmlReturn_t result = nvmlDeviceGetMigMode(parentDevices[deviceIndex], &currentMode, &pendingMode);
        return (result == NVML_SUCCESS && currentMode == 1);
    }
    
    // 사용 가능한 GPU Instance 프로파일 조회
    std::vector<GPUInstanceProfile> getAvailableInstanceProfiles(unsigned int deviceIndex) {
        std::vector<GPUInstanceProfile> profiles;
        
        if (deviceIndex >= parentDevices.size()) return profiles;
        
        unsigned int profileCount = 0;
        nvmlGpuInstanceProfileInfo_t profileInfos[NVML_GPU_INSTANCE_PROFILE_COUNT];
        
        nvmlReturn_t result = nvmlDeviceGetGpuInstanceProfiles(parentDevices[deviceIndex], 
                                                              NVML_GPU_INSTANCE_PROFILE_COUNT,
                                                              profileInfos, &profileCount);
        
        if (result == NVML_SUCCESS) {
            for (unsigned int i = 0; i < profileCount; i++) {
                GPUInstanceProfile profile;
                profile.profileId = profileInfos[i].id;
                profile.memorySizeMB = profileInfos[i].memorySizeMB;
                profile.multiprocessorCount = profileInfos[i].multiprocessorCount;
                profile.maxComputeInstances = profileInfos[i].maxComputeInstances;
                // 프로파일 이름은 ID 기반으로 생성
                profile.name = "Profile_" + std::to_string(profile.profileId);
                profiles.push_back(profile);
            }
        }
        
        return profiles;
    }
    
    // GPU Instance 생성
    bool createGPUInstance(unsigned int deviceIndex, unsigned int profileId, unsigned int& instanceId) {
        if (deviceIndex >= parentDevices.size()) return false;
        
        nvmlGpuInstance_t gpuInstance;
        nvmlReturn_t result = nvmlDeviceCreateGpuInstance(parentDevices[deviceIndex], profileId, &gpuInstance);
        
        if (result == NVML_SUCCESS) {
            // Instance ID 가져오기
            nvmlGpuInstanceInfo_t instanceInfo;
            if (nvmlGpuInstanceGetInfo(gpuInstance, &instanceInfo) == NVML_SUCCESS) {
                instanceId = instanceInfo.id;
                return true;
            }
        }
        
        return false;
    }
    
    // GPU Instance 삭제
    bool destroyGPUInstance(unsigned int deviceIndex, unsigned int instanceId) {
        if (deviceIndex >= parentDevices.size()) return false;
        
        nvmlGpuInstance_t gpuInstance;
        nvmlReturn_t result = nvmlDeviceGetGpuInstanceById(parentDevices[deviceIndex], instanceId, &gpuInstance);
        
        if (result == NVML_SUCCESS) {
            result = nvmlGpuInstanceDestroy(gpuInstance);
            return result == NVML_SUCCESS;
        }
        
        return false;
    }
    
    // 모든 GPU Instance 조회
    std::vector<MIGDeviceInfo> getAllGPUInstances(unsigned int deviceIndex) {
        std::vector<MIGDeviceInfo> instances;
        
        if (deviceIndex >= parentDevices.size() || !isMIGModeEnabled(deviceIndex)) {
            return instances;
        }
        
        unsigned int count = 0;
        nvmlGpuInstance_t gpuInstances[NVML_GPU_INSTANCE_PROFILE_COUNT];
        
        nvmlReturn_t result = nvmlDeviceGetGpuInstances(parentDevices[deviceIndex], 
                                                       NVML_GPU_INSTANCE_PROFILE_COUNT,
                                                       gpuInstances, &count);
        
        if (result == NVML_SUCCESS) {
            for (unsigned int i = 0; i < count; i++) {
                MIGDeviceInfo migInfo;
                
                nvmlGpuInstanceInfo_t instanceInfo;
                if (nvmlGpuInstanceGetInfo(gpuInstances[i], &instanceInfo) == NVML_SUCCESS) {
                    migInfo.instanceId = instanceInfo.id;
                    
                    // GPU Instance에서 디바이스 핸들 가져오기
                    nvmlDevice_t migDevice;
                    if (nvmlGpuInstanceGetComputeInstanceById(gpuInstances[i], 0, &migDevice) == NVML_SUCCESS) {
                        migInfo.migDevice = migDevice;
                        
                        // 디바이스 속성 가져오기
                        nvmlDeviceGetAttributes(migDevice, &migInfo.attributes);
                        
                        // UUID 가져오기
                        char uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
                        if (nvmlDeviceGetUUID(migDevice, uuid, sizeof(uuid)) == NVML_SUCCESS) {
                            migInfo.uuid = uuid;
                        }
                        
                        // 메모리 정보
                        nvmlMemory_t memInfo;
                        if (nvmlDeviceGetMemoryInfo(migDevice, &memInfo) == NVML_SUCCESS) {
                            migInfo.memorySize = memInfo.total;
                        }
                    }
                    
                    instances.push_back(migInfo);
                }
            }
        }
        
        return instances;
    }
    
    // Compute Instance 생성
    bool createComputeInstance(unsigned int deviceIndex, unsigned int gpuInstanceId, 
                              unsigned int profileId, unsigned int& computeInstanceId) {
        if (deviceIndex >= parentDevices.size()) return false;
        
        nvmlGpuInstance_t gpuInstance;
        nvmlReturn_t result = nvmlDeviceGetGpuInstanceById(parentDevices[deviceIndex], gpuInstanceId, &gpuInstance);
        
        if (result == NVML_SUCCESS) {
            nvmlComputeInstance_t computeInstance;
            result = nvmlGpuInstanceCreateComputeInstance(gpuInstance, profileId, &computeInstance);
            
            if (result == NVML_SUCCESS) {
                nvmlComputeInstanceInfo_t computeInfo;
                if (nvmlComputeInstanceGetInfo(computeInstance, &computeInfo) == NVML_SUCCESS) {
                    computeInstanceId = computeInfo.id;
                    return true;
                }
            }
        }
        
        return false;
    }
    
    // MIG 디바이스의 메트릭 수집
    GPUMetrics getMIGDeviceMetrics(const MIGDeviceInfo& migDevice) {
        GPUMetrics metrics = {};
        metrics.timestamp = std::chrono::system_clock::now();
        
        // 사용률 정보
        nvmlUtilization_t utilization;
        if (nvmlDeviceGetUtilizationRates(migDevice.migDevice, &utilization) == NVML_SUCCESS) {
            metrics.gpuUtilization = utilization.gpu;
            metrics.memoryUtilization = utilization.memory;
        }
        
        // 메모리 정보
        nvmlMemory_t memInfo;
        if (nvmlDeviceGetMemoryInfo(migDevice.migDevice, &memInfo) == NVML_SUCCESS) {
            metrics.memoryUsed = memInfo.used;
            metrics.memoryFree = memInfo.free;
            metrics.memoryTotal = memInfo.total;
        }
        
        // 전력 사용량 (부모 디바이스와 공유)
        nvmlDeviceGetPowerUsage(migDevice.migDevice, &metrics.powerUsage);
        
        // 온도 (부모 디바이스와 공유)
        nvmlDeviceGetTemperature(migDevice.migDevice, NVML_TEMPERATURE_GPU, &metrics.temperature);
        
        return metrics;
    }
    
    // 모든 MIG 디바이스의 메트릭 수집
    std::map<std::string, GPUMetrics> getAllMIGMetrics() {
        std::map<std::string, GPUMetrics> allMetrics;
        
        for (size_t i = 0; i < parentDevices.size(); i++) {
            if (isMIGModeEnabled(i)) {
                auto instances = getAllGPUInstances(i);
                for (const auto& instance : instances) {
                    std::string key = "GPU" + std::to_string(i) + "_MIG" + std::to_string(instance.instanceId);
                    allMetrics[key] = getMIGDeviceMetrics(instance);
                }
            }
        }
        
        return allMetrics;
    }
};