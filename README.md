# 설치 및 빌드 방법

# 1. 필수 조건 설치 (Ubuntu/Debian)

sudo apt-get update
sudo apt-get install build-essential cmake
sudo apt-get install nvidia-driver-dev # NVML 헤더 포함
sudo apt-get install cuda-toolkit # CUDA 툴킷

# 2. 빌드 실행

chmod +x build.sh
./build.sh

# 3. 실행

cd build
./nvml_monitoring

# 4. 시스템 전체 설치 (선택사항)

sudo make install

# 또는

sudo cp nvml_monitoring /usr/local/bin/

# 사용 예제들:

# 기본 모니터링 실행

./nvml_monitoring

# 특정 GPU만 모니터링

./nvml_monitoring --gpu 0

# 출력을 파일로 저장

./nvml_monitoring > gpu_monitor.log 2>&1

# 백그라운드 실행

nohup ./nvml_monitoring &

# systemd 서비스로 등록

sudo cp nvml_monitoring.service /etc/systemd/system/
sudo systemctl enable nvml_monitoring
sudo systemctl start nvml_monitoring
