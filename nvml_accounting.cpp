#include "nvml_types.h"
#include <vector>
#include <chrono>

struct AccountingStats {
    unsigned int gpuUtilization;
    unsigned long long memoryUtilization;
    unsigned long long maxMemoryUsage;
    unsigned long long time;
    unsigned long long startTime;
    bool isRunning;
    unsigned int reserved[5];
};

struct ProcessAccountingStats {
    unsigned int pid;
    unsigned long long maxMemoryUsage;
    unsigned long long time;
    unsigned long long startTime;
    bool isRunning;
    std::string processName;
    std::vector<AccountingStats> samples;
};

class NVMLAccounting {
private:
    std::vector<nvmlDevice_t> devices;
    bool accountingEnabled = false;
    
public:
    NVMLAccounting(const std::vector<nvmlDevice_t>& deviceList) 
        : devices(deviceList) {}
    
    // Accounting 모드 활성화
    bool enableAccounting(unsigned int deviceIndex) {
        if (deviceIndex >= devices.size()) return false;
        
        nvmlReturn_t result = nvmlDeviceSetAccountingMode(devices[deviceIndex], NVML_FEATURE_ENABLED);
        if (result == NVML_SUCCESS) {
            accountingEnabled = true;
            return true;
        }
        return false;
    }
    
    // Accounting 모드 비활성화
    bool disableAccounting(unsigned int deviceIndex) {
        if (deviceIndex >= devices.size()) return false;
        
        nvmlReturn_t result = nvmlDeviceSetAccountingMode(devices[deviceIndex], NVML_FEATURE_DISABLED);
        return result == NVML_SUCCESS;
    }
    
    // Accounting 모드 상태 확인
    bool isAccountingEnabled(unsigned int deviceIndex) {
        if (deviceIndex >= devices.size()) return false;
        
        nvmlEnableState_t mode;
        nvmlReturn_t result = nvmlDeviceGetAccountingMode(devices[deviceIndex], &mode);
        return (result == NVML_SUCCESS && mode == NVML_FEATURE_ENABLED);
    }
    
    // 현재 실행 중인 프로세스들의 Accounting 정보
    std::vector<ProcessAccountingStats> getRunningProcessStats(unsigned int deviceIndex) {
        std::vector<ProcessAccountingStats> processStats;
        
        if (deviceIndex >= devices.size() || !isAccountingEnabled(deviceIndex)) {
            return processStats;
        }
        
        // 실행 중인 프로세스 목록 가져오기
        unsigned int infoCount = 0;
        nvmlDeviceGetAccountingPids(devices[deviceIndex], &infoCount, nullptr);
        
        if (infoCount == 0) return processStats;
        
        std::vector<unsigned int> pids(infoCount);
        nvmlReturn_t result = nvmlDeviceGetAccountingPids(devices[deviceIndex], &infoCount, pids.data());
        
        if (result != NVML_SUCCESS) return processStats;
        
        for (unsigned int pid : pids) {
            nvmlAccountingStats_t stats;
            result = nvmlDeviceGetAccountingStats(devices[deviceIndex], pid, &stats);
            
            if (result == NVML_SUCCESS) {
                ProcessAccountingStats procStats;
                procStats.pid = pid;
                procStats.maxMemoryUsage = stats.maxMemoryUsage;
                procStats.time = stats.time;
                procStats.startTime = stats.startTime;
                procStats.isRunning = stats.isRunning;
                
                // 프로세스 이름 가져오기
                char processName[1024];
                if (nvmlSystemGetProcessName(pid, processName, sizeof(processName)) == NVML_SUCCESS) {
                    procStats.processName = processName;
                }
                
                processStats.push_back(procStats);
            }
        }
        
        return processStats;
    }
    
    // 특정 프로세스의 상세 Accounting 정보
    ProcessAccountingStats getProcessAccountingStats(unsigned int deviceIndex, unsigned int pid) {
        ProcessAccountingStats procStats;
        procStats.pid = pid;
        
        if (deviceIndex >= devices.size() || !isAccountingEnabled(deviceIndex)) {
            return procStats;
        }
        
        nvmlAccountingStats_t stats;
        nvmlReturn_t result = nvmlDeviceGetAccountingStats(devices[deviceIndex], pid, &stats);
        
        if (result == NVML_SUCCESS) {
            procStats.maxMemoryUsage = stats.maxMemoryUsage;
            procStats.time = stats.time;
            procStats.startTime = stats.startTime;
            procStats.isRunning = stats.isRunning;
            
            char processName[1024];
            if (nvmlSystemGetProcessName(pid, processName, sizeof(processName)) == NVML_SUCCESS) {
                procStats.processName = processName;
            }
        }
        
        return procStats;
    }
    
    // Accounting 버퍼 크기 조회
    unsigned int getAccountingBufferSize(unsigned int deviceIndex) {
        if (deviceIndex >= devices.size()) return 0;
        
        unsigned int bufferSize;
        nvmlReturn_t result = nvmlDeviceGetAccountingBufferSize(devices[deviceIndex], &bufferSize);
        return (result == NVML_SUCCESS) ? bufferSize : 0;
    }
    
    // Accounting 통계 지우기
    bool clearAccountingPids(unsigned int deviceIndex) {
        if (deviceIndex >= devices.size()) return false;
        
        nvmlReturn_t result = nvmlDeviceClearAccountingPids(devices[deviceIndex]);
        return result == NVML_SUCCESS;
    }
    
    // 모든 디바이스의 Accounting 정보 수집
    std::map<unsigned int, std::vector<ProcessAccountingStats>> getAllDeviceAccountingStats() {
        std::map<unsigned int, std::vector<ProcessAccountingStats>> allStats;
        
        for (size_t i = 0; i < devices.size(); i++) {
            if (isAccountingEnabled(i)) {
                allStats[i] = getRunningProcessStats(i);
            }
        }
        
        return allStats;
    }
    
    // Accounting 정보를 주기적으로 수집하는 함수
    void startPeriodicCollection(int intervalSeconds, 
                                std::function<void(const std::map<unsigned int, std::vector<ProcessAccountingStats>>&)> callback) {
        std::thread([this, intervalSeconds, callback]() {
            while (accountingEnabled) {
                auto stats = getAllDeviceAccountingStats();
                if (callback && !stats.empty()) {
                    callback(stats);
                }
                std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
            }
        }).detach();
    }
};