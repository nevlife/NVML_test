#pragma once

#include <nvml.h>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <optional>

namespace nvml_mig {

// 오류 처리를 위한 예외 클래스
class NVMLException : public std::exception {
private:
    nvmlReturn_t error;
    std::string message;

public:
    NVMLException(nvmlReturn_t err, const std::string& context) 
        : error(err), message(context + ": " + nvmlErrorString(err)) {}
    
    const char* what() const noexcept override {
        return message.c_str();
    }
    
    nvmlReturn_t getError() const {
        return error;
    }
};

// MIG 메트릭 데이터 구조체
struct MIGMetrics {
    std::chrono::system_clock::time_point timestamp;
    unsigned int gpuUtilization;
    unsigned int memoryUtilization;
    unsigned long long memoryUsed;
    unsigned long long memoryFree;
    unsigned long long memoryTotal;
    unsigned int powerUsage;
    unsigned int temperature;
    std::map<std::string, unsigned int> processUtilization;
};

// MIG 디바이스 정보 구조체
struct MIGDeviceInfo {
    nvmlDevice_t deviceHandle;
    unsigned int parentDeviceIndex;
    unsigned int instanceId;
    unsigned int profileId;
    std::string uuid;
    unsigned long long memorySize;
    unsigned int multiprocessorCount;
    unsigned int maxComputeInstances;
    unsigned int currentComputeInstances;
    std::vector<unsigned int> computeInstanceIds;
};

// GPU 인스턴스 프로파일 정보
struct MIGProfile {
    unsigned int profileId;
    unsigned long long memorySizeMB;
    unsigned int multiprocessorCount;
    unsigned int maxComputeInstances;
    std::string name;
};

// NVML 관련 유틸리티 함수들
class NVMLUtil {
public:
    // RAII 패턴을 사용한 NVML 초기화/종료
    class NVMLGuard {
    public:
        NVMLGuard() {
            nvmlReturn_t result = nvmlInit();
            if (result != NVML_SUCCESS) {
                throw NVMLException(result, "Failed to initialize NVML");
            }
        }
        
        ~NVMLGuard() {
            nvmlShutdown();
        }
        
        // 복사 및 이동 금지
        NVMLGuard(const NVMLGuard&) = delete;
        NVMLGuard& operator=(const NVMLGuard&) = delete;
        NVMLGuard(NVMLGuard&&) = delete;
        NVMLGuard& operator=(NVMLGuard&&) = delete;
    };
};

// MIG 관리자 클래스 - 싱글톤 패턴 적용
class MIGManager {
private:
    static std::unique_ptr<MIGManager> instance;
    static std::mutex instanceMutex;
    
    std::unique_ptr<NVMLUtil::NVMLGuard> nvmlGuard;
    std::vector<nvmlDevice_t> devices;
    std::map<std::string, MIGDeviceInfo> migDevices; // UUID를 키로 사용
    
    std::atomic<bool> monitoringActive;
    std::thread monitoringThread;
    std::mutex metricsMutex;
    std::condition_variable monitoringCV;
    std::map<std::string, MIGMetrics> latestMetrics;
    
    // 비동기 작업 큐
    struct AsyncTask {
        std::function<void()> task;
        std::function<void(bool, const std::string&)> callback;
    };
    
    std::queue<AsyncTask> taskQueue;
    std::mutex taskMutex;
    std::condition_variable taskCV;
    std::thread workerThread;
    std::atomic<bool> workerActive;
    
    // 생성자는 private으로 (싱글톤)
    MIGManager();
    
    // GPU 디바이스 초기화
    void initializeDevices();
    
    // 모니터링 스레드 함수
    void monitoringLoop();
    
    // 작업자 스레드 함수
    void workerLoop();
    
    // MIG 디바이스 메트릭 수집
    MIGMetrics collectDeviceMetrics(const MIGDeviceInfo& device);
    
public:
    ~MIGManager();
    
    // 싱글톤 인스턴스 접근
    static MIGManager& getInstance();
    
    // MIG 모드 활성화
    bool enableMIGMode(unsigned int deviceIndex, bool async = false, 
                      std::function<void(bool, const std::string&)> callback = nullptr);
    
    // MIG 모드 비활성화
    bool disableMIGMode(unsigned int deviceIndex, bool async = false,
                       std::function<void(bool, const std::string&)> callback = nullptr);
    
    // MIG 모드 상태 확인
    bool isMIGModeEnabled(unsigned int deviceIndex);
    
    // GPU 인스턴스 프로파일 조회
    std::vector<MIGProfile> getAvailableProfiles(unsigned int deviceIndex);
    
    // GPU 인스턴스 생성
    bool createGPUInstance(unsigned int deviceIndex, unsigned int profileId, unsigned int& instanceId, 
                          bool async = false, std::function<void(bool, const std::string&)> callback = nullptr);
    
    // GPU 인스턴스 삭제
    bool destroyGPUInstance(unsigned int deviceIndex, unsigned int instanceId,
                           bool async = false, std::function<void(bool, const std::string&)> callback = nullptr);
    
    // 컴퓨트 인스턴스 생성
    bool createComputeInstance(unsigned int deviceIndex, unsigned int gpuInstanceId, 
                              unsigned int profileId, unsigned int& computeInstanceId,
                              bool async = false, std::function<void(bool, const std::string&)> callback = nullptr);
    
    // 모든 MIG 디바이스 정보 조회
    std::vector<MIGDeviceInfo> getAllMIGDevices();
    
    // 특정 디바이스의 MIG 인스턴스 조회
    std::vector<MIGDeviceInfo> getMIGDevices(unsigned int deviceIndex);
    
    // UUID로 MIG 디바이스 찾기
    std::optional<MIGDeviceInfo> findMIGDeviceByUUID(const std::string& uuid);
    
    // MIG 디바이스 메트릭 조회
    std::optional<MIGMetrics> getMIGDeviceMetrics(const std::string& uuid);
    
    // 모든 MIG 디바이스 메트릭 조회
    std::map<std::string, MIGMetrics> getAllMIGMetrics();
    
    // 모니터링 시작
    void startMonitoring(unsigned int intervalMs = 1000);
    
    // 모니터링 중지
    void stopMonitoring();
    
    // 장치 개수 조회
    size_t getDeviceCount() const;
    
    // 장치 정보 조회
    nvmlDevice_t getDeviceHandle(unsigned int index) const;
    
    // 장치 이름 조회
    std::string getDeviceName(unsigned int index) const;
    
    // 현재 MIG 구성 저장 (JSON 형식)
    bool saveMIGConfiguration(const std::string& filePath);
    
    // 저장된 MIG 구성 적용
    bool loadMIGConfiguration(const std::string& filePath, 
                             bool async = false, std::function<void(bool, const std::string&)> callback = nullptr);
};

// 유틸리티 함수들
namespace utils {
    // MIG 구성을 JSON으로 변환
    std::string migConfigToJson(const std::vector<MIGDeviceInfo>& devices);
    
    // JSON에서 MIG 구성 파싱
    std::vector<std::pair<unsigned int, std::vector<unsigned int>>> parseMigConfigFromJson(const std::string& json);
    
    // MIG 구성 비교 (현재 vs 목표)
    bool compareMIGConfigurations(const std::vector<MIGDeviceInfo>& current, 
                                 const std::vector<std::pair<unsigned int, std::vector<unsigned int>>>& target);
}

} // namespace nvml_mig 