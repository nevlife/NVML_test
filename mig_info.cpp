#include "nvml_mig_optimal.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>

using namespace nvml_mig;

// 단위 변환 유틸리티 함수
std::string bytesToString(unsigned long long bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = bytes;
    
    while (size > 1024 && unitIndex < 4) {
        size /= 1024;
        unitIndex++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return ss.str();
}

// 시간 포맷팅 함수
std::string formatTime(const std::chrono::system_clock::time_point& time) {
    auto tt = std::chrono::system_clock::to_time_t(time);
    std::tm* now = std::localtime(&tt);
    
    std::stringstream ss;
    ss << std::put_time(now, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// MIG 디바이스 정보 출력
void printMIGDeviceInfo(const MIGDeviceInfo& device) {
    std::cout << "---------------------------------------------" << std::endl;
    std::cout << "MIG 인스턴스 ID: " << device.instanceId << std::endl;
    std::cout << "프로파일 ID: " << device.profileId << std::endl;
    std::cout << "UUID: " << device.uuid << std::endl;
    std::cout << "메모리 크기: " << bytesToString(device.memorySize) << std::endl;
    std::cout << "멀티프로세서 수: " << device.multiprocessorCount << std::endl;
    std::cout << "최대 컴퓨트 인스턴스 수: " << device.maxComputeInstances << std::endl;
    std::cout << "현재 컴퓨트 인스턴스 수: " << device.currentComputeInstances << std::endl;
    
    std::cout << "컴퓨트 인스턴스 IDs: ";
    for (size_t i = 0; i < device.computeInstanceIds.size(); i++) {
        std::cout << device.computeInstanceIds[i];
        if (i < device.computeInstanceIds.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << std::endl;
}

// MIG 메트릭 출력
void printMIGMetrics(const std::string& uuid, const MIGMetrics& metrics) {
    std::cout << "---------------------------------------------" << std::endl;
    std::cout << "MIG 인스턴스 UUID: " << uuid << std::endl;
    std::cout << "측정 시간: " << formatTime(metrics.timestamp) << std::endl;
    std::cout << "GPU 사용률: " << metrics.gpuUtilization << "%" << std::endl;
    std::cout << "메모리 사용률: " << metrics.memoryUtilization << "%" << std::endl;
    std::cout << "메모리 사용: " << bytesToString(metrics.memoryUsed) << " / " 
              << bytesToString(metrics.memoryTotal) << std::endl;
    std::cout << "전력 사용량: " << metrics.powerUsage / 1000.0 << "W" << std::endl;
    std::cout << "온도: " << metrics.temperature << "°C" << std::endl;
    
    if (!metrics.processUtilization.empty()) {
        std::cout << "실행 중인 프로세스:" << std::endl;
        for (const auto& [name, memory] : metrics.processUtilization) {
            std::cout << "  - " << name << ": " << memory << " MB" << std::endl;
        }
    }
}

// GPU 인스턴스 프로파일 출력
void printMIGProfiles(unsigned int deviceIndex, const std::vector<MIGProfile>& profiles) {
    std::cout << "---------------------------------------------" << std::endl;
    std::cout << "GPU " << deviceIndex << " 지원 MIG 프로파일:" << std::endl;
    
    for (const auto& profile : profiles) {
        std::cout << "  프로파일 " << profile.profileId << ": " << profile.name << std::endl;
        std::cout << "    메모리: " << profile.memorySizeMB << " MB" << std::endl;
        std::cout << "    멀티프로세서: " << profile.multiprocessorCount << std::endl;
        std::cout << "    최대 컴퓨트 인스턴스: " << profile.maxComputeInstances << std::endl;
        std::cout << std::endl;
    }
}

// 모니터링 모드 실행
void runMonitoringMode(MIGManager& manager, unsigned int intervalSec) {
    std::cout << "MIG 모니터링 모드 시작 (간격: " << intervalSec << "초, Ctrl+C로 종료)" << std::endl;
    
    // 모니터링 시작
    manager.startMonitoring();
    
    try {
        while (true) {
            std::cout << "\033[2J\033[1;1H"; // 화면 지우기 (ANSI 이스케이프 코드)
            std::cout << "MIG 상태 모니터링 (갱신: " << formatTime(std::chrono::system_clock::now()) << ")" << std::endl;
            
            // 모든 디바이스 정보 조회
            for (size_t i = 0; i < manager.getDeviceCount(); i++) {
                std::cout << "==============================================" << std::endl;
                std::cout << "GPU " << i << ": " << manager.getDeviceName(i) << std::endl;
                
                // MIG 모드 상태 확인
                bool migEnabled = manager.isMIGModeEnabled(i);
                std::cout << "MIG 모드: " << (migEnabled ? "활성화됨" : "비활성화됨") << std::endl;
                
                if (migEnabled) {
                    // MIG 인스턴스 조회
                    auto devices = manager.getMIGDevices(i);
                    std::cout << "MIG 인스턴스 수: " << devices.size() << std::endl;
                    
                    // 각 인스턴스의 메트릭 출력
                    for (const auto& device : devices) {
                        auto metrics = manager.getMIGDeviceMetrics(device.uuid);
                        if (metrics) {
                            printMIGMetrics(device.uuid, *metrics);
                        }
                    }
                }
            }
            
            // 일정 시간 대기
            std::this_thread::sleep_for(std::chrono::seconds(intervalSec));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "모니터링 중 오류 발생: " << e.what() << std::endl;
    }
    
    // 모니터링 중지
    manager.stopMonitoring();
}

int main(int argc, char** argv) {
    try {
        // MIG 관리자 인스턴스 가져오기
        MIGManager& manager = MIGManager::getInstance();
        
        std::cout << "NVIDIA MIG 정보 조회 도구" << std::endl;
        std::cout << "==============================================" << std::endl;
        
        // 디바이스 개수 확인
        size_t deviceCount = manager.getDeviceCount();
        std::cout << "발견된 NVIDIA GPU: " << deviceCount << "개" << std::endl;
        
        if (deviceCount == 0) {
            std::cout << "NVIDIA GPU를 찾을 수 없습니다." << std::endl;
            return 0;
        }
        
        // 각 디바이스 정보 출력
        for (size_t i = 0; i < deviceCount; i++) {
            std::cout << "==============================================" << std::endl;
            std::cout << "GPU " << i << ": " << manager.getDeviceName(i) << std::endl;
            
            // MIG 모드 상태 확인
            bool migEnabled = manager.isMIGModeEnabled(i);
            std::cout << "MIG 모드: " << (migEnabled ? "활성화됨" : "비활성화됨") << std::endl;
            
            if (migEnabled) {
                // MIG 인스턴스 조회
                auto devices = manager.getMIGDevices(i);
                std::cout << "MIG 인스턴스 수: " << devices.size() << std::endl;
                
                // 각 인스턴스 정보 출력
                for (const auto& device : devices) {
                    printMIGDeviceInfo(device);
                    
                    // 인스턴스 메트릭 출력
                    auto metrics = manager.getMIGDeviceMetrics(device.uuid);
                    if (metrics) {
                        printMIGMetrics(device.uuid, *metrics);
                    }
                }
            }
            else {
                // 사용 가능한 MIG 프로파일 조회
                auto profiles = manager.getAvailableProfiles(i);
                printMIGProfiles(i, profiles);
            }
        }
        
        // 인자가 있으면 모니터링 모드 실행
        if (argc > 1 && std::string(argv[1]) == "--monitor") {
            unsigned int interval = 5; // 기본 간격 5초
            if (argc > 2) {
                try {
                    interval = std::stoi(argv[2]);
                }
                catch (...) {
                    // 잘못된 간격이면 기본값 사용
                }
            }
            
            runMonitoringMode(manager, interval);
        }
        
        return 0;
    }
    catch (const NVMLException& e) {
        std::cerr << "NVML 오류: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "오류 발생: " << e.what() << std::endl;
        return 1;
    }
} 