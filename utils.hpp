#include <fstream>
#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <memory>
#include <cstring>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sstream>
#include <numeric>
#include <regex>
#include <dirent.h>
#include <algorithm>
#include "json.hpp"

  const std::string Log_path = "/sdcard/Android/MW_CpuSpeedController/log.txt"; // 日志文件
  const std::string config_path = "/sdcard/Android/MW_CpuSpeedController/config.txt"; // 配置文件变化
  const std::string json_path = "/sdcard/Android/MW_CpuSpeedController/config.json"; // 后续的配置文件
    /*接下来都是针对CPU调度的路径*/
  const std::string efficient_freq = "/sys/devices/system/cpu/cpufreq/policy0/schedhorizon/efficient_freqn";
  const std::string up_delay = "/sys/devices/system/cpu/cpufreq/policy0/schedhorizon/up_delay";
  const std::string scaling_min_freq_limit = "/sys/devices/system/cpu/cpufreq/policy0/scaling_min_freq_limit";
  const std::string cpu_uclamp_min = "/dev/cpuctl/top-app/cpu.uclamp.min";
  const std::string cpu_uclamp_max = "/dev/cpuctl/top-app/cpu.uclamp.max";
  const std::string foreground_cpu_uclamp_min = "/dev/cpuctl/foreground/cpu.uclamp.min";
  const std::string foreground_cpu_uclamp_max = "/dev/cpuctl/foreground/cpu.uclamp.max";
  const std::string policy0 = "/sys/devices/system/cpu/cpufreq/policy0/schedhorizon/up_delay";
  const std::string policy4 = "/sys/devices/system/cpu/cpufreq/policy4/schedhorizon/up_delay";
  const std::string policy7 = "/sys/devices/system/cpu/cpufreq/policy7/schedhorizon/up_delay";
  const std::string cpu_min_freq = "/sys/kernel/msm_performance/parameters/cpu_min_freq";
  const std::string scaling_min_freq0 = "/sys/devices/system/cpu/cpufreq/policy0/scaling_min_freq";
  const std::string scaling_min_freq4 = "/sys/devices/system/cpu/cpufreq/policy4/scaling_min_freq";
  const std::string scaling_min_freq7 = "/sys/devices/system/cpu/cpufreq/policy7/scaling_min_freq";
  // 下面都是核心绑定
  const std::string background_cpu = "/dev/cpuset/background/cpus"; // 用户的后台应用
  const std::string system_background_cpu = "/dev/cpuset/system-background/cpus"; // 系统的后台应用
  const std::string foreground_cpu = "/dev/cpuset/foreground/cpus"; // 前台的应用
  const std::string top_app = "/dev/cpuset/top-app/cpus"; // 顶层应用
  using json = nlohmann::json;
  std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return content;
}

// 去除字符串两端的空白字符
std::string TrimStr(std::string str) {
    str.erase(0, str.find_first_not_of(' ')); // 前导空格
    str.erase(str.find_last_not_of(' ') + 1); // 尾随空格
    return str;
}

/*inline void Init() {
    // 检查是否还有其他CPU调速器进程，防止进程多开
    char buf[256] = { 0 };
    if (popenRead("pidof MW_CpuSpeedController", buf, sizeof(buf)) == 0) {
        printException(nullptr, 0, "进程检测失败", 18);
        exit(-1);
    }

    // 检查是否有多个PID
    if (strchr(buf, ' ') != nullptr) {
        char tips[256];
        auto len = snprintf(tips, sizeof(tips),
            "CPU调速器已经在运行(pids: %s), 当前进程(pid:%d)即将退出，"
            "请勿手动启动调速器, 也不要在多个框架同时启动CPU调速器",
            buf, getpid());
        printf("\n!!! \n!!! %s\n!!!\n\n", tips);
        printException(nullptr, 0, tips, len);
        exit(-2);
    }
}*/
inline void Log(const std::string& message) {
    std::time_t now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    char time_str[100];
    std::strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S]", local_time);

    std::ofstream logfile(Log_path, std::ios_base::app);
    if (logfile.is_open()) {
        logfile << time_str << " " << message << std::endl;
        logfile.close();
    }
}
inline void WriteFile(const std::string &filePath, const std::string &content) noexcept
{

    int fd = open(filePath.c_str(), O_WRONLY | O_NONBLOCK);

    if (fd < 0) {
        chmod(filePath.c_str(), 0666);
        fd = open(filePath.c_str(), O_WRONLY | O_NONBLOCK); // 如果写入失败将会授予0666权限
    }
    if (fd >= 0) {
        write(fd, content.data(), content.size());
        close(fd); // 写入成功后关闭文件 防止调速器恢复授予0444权限
        chmod(filePath.c_str(), 0444);
    }
}
inline void disable_qcomGpuBoost(){
    std::string num_pwrlevels_path = "/sys/class/kgsl/kgsl-3d0/num_pwrlevels";
    std::ifstream file(num_pwrlevels_path);
    int num_pwrlevels;
    if (file >> num_pwrlevels) {
        int MIN_PWRLVL = num_pwrlevels - 1;
        std::string minPwrlvlStr = std::to_string(MIN_PWRLVL);
        Log("已关闭高通GPU Boost");
        WriteFile("/sys/class/kgsl/kgsl-3d0/default_pwrlevel", minPwrlvlStr);
        WriteFile("/sys/class/kgsl/kgsl-3d0/min_pwrlevel", minPwrlvlStr);
        WriteFile("/sys/class/kgsl/kgsl-3d0/max_pwrlevel", "0");
        WriteFile("/sys/class/kgsl/kgsl-3d0/thermal_pwrlevel", "0");   
        WriteFile("/sys/class/kgsl/kgsl-3d0/throttling", "0");
    }
}
inline int InotifyMain(const char *dir_name, uint32_t mask) {
    int fd = inotify_init();
    if (fd < 0) {
        std::cerr << "Failed to initialize inotify." << std::endl;
        return -1;
    }

    int wd = inotify_add_watch(fd, dir_name, mask);
    if (wd < 0) {
        std::cerr << "Failed to add watch for directory: " << dir_name << std::endl;
        close(fd);
        return -1;
    }

    const int buflen = sizeof(struct inotify_event) + NAME_MAX + 1;
    char buf[buflen];
    fd_set readfds;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        int iRet = select(fd + 1, &readfds, nullptr, nullptr, nullptr);
        if (iRet < 0) {
            break;
        }

        int len = read(fd, buf, buflen);
        if (len < 0) {
            std::cerr << "Failed to read inotify events." << std::endl;
            break;
        }

        const struct inotify_event *event = reinterpret_cast<const struct inotify_event *>(buf);
        if (event->mask & mask) {
            break;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);

    return 0;
}
std::vector<std::string> GpuDDR() {
    std::vector<std::string> tripPointPaths;
    std::regex cpuThermalRegex("cpu|gpu|dd");

    auto dir = opendir("/sys/class/thermal");
    if (dir) {
        for (auto entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
            std::string dirName(entry->d_name);
            if (dirName.find("thermal_zone") != std::string::npos) {
                auto type = TrimStr(ReadFile("/sys/class/thermal/" + dirName + "/type"));
                if (std::regex_match(type, cpuThermalRegex)) {
                    DIR *zoneDir = opendir(("/sys/class/thermal/" + dirName).c_str());
                    if (zoneDir) {
                        for (auto zoneEntry = readdir(zoneDir); zoneEntry != nullptr; zoneEntry = readdir(zoneDir)) {
                            std::string fileName(zoneEntry->d_name);
                            if (fileName.find("trip_point_") == 0) {
                                std::string tripPointPath = "/sys/class/thermal/" + dirName + "/" + fileName;
                                tripPointPaths.push_back(tripPointPath);
                            }
                        }
                        closedir(zoneDir);
                    }
                    break;
                }
            }
        }
        closedir(dir);
    }
    return tripPointPaths;
}
inline void core_allocation() {
        WriteFile(background_cpu, "0-1");
        WriteFile(system_background_cpu, "0-2");
        WriteFile(foreground_cpu, "0-6");
        WriteFile(top_app, "0-7");
}
inline void writePolicyValues(int policyStart, int policyEnd, const std::vector<std::string>& freqs, const std::vector<int>& upDelays, int minFreqLimit) {
    for(int i = policyStart; i <= policyEnd; ++i) {
        std::stringstream ss;
        ss << "/sys/devices/system/cpu/cpufreq/policy" << i << "/schedhorizon/efficient_freq";
        WriteFile(ss.str(), accumulate(freqs.begin(), freqs.end(), std::string{}));
        
        ss.str("");  //清空stringstream以便再次使用
        ss << "/sys/devices/system/cpu/cpufreq/policy" << i << "/schedhorizon/up_delay";
        std::stringstream delayStream;
        for(auto delay : upDelays) {
            delayStream << delay << " ";
        }
        WriteFile(ss.str(), delayStream.str());
        
        ss.str("");
        ss << "/sys/devices/system/cpu/cpufreq/policy" << i << "/scaling_min_freq_limit";
        WriteFile(ss.str(), std::to_string(minFreqLimit));
    }
}
inline void enableFeas(){
  Log("Feas已启用"); // 恢复原本参数 让系统开启官调
  WriteFile(cpu_uclamp_min, "20");
  WriteFile(cpu_uclamp_max, "max");
  WriteFile(foreground_cpu_uclamp_min, "10");
  WriteFile(foreground_cpu_uclamp_max, "80");
  WriteFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "sugov_ext");
  WriteFile("/sys/devices/system/cpu/cpu4/cpufreq/scaling_governor", "sugov_ext");
  WriteFile("/sys/devices/system/cpu/cpu7/cpufreq/scaling_governor", "sugov_ext");
  WriteFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "walt");
  WriteFile("/sys/devices/system/cpu/cpu4/cpufreq/scaling_governor", "walt");
  WriteFile("/sys/devices/system/cpu/cpu7/cpufreq/scaling_governor", "walt");
  WriteFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", "0");
  WriteFile("/sys/devices/system/cpu/cpu4/cpufreq/scaling_min_freq", "0");
  WriteFile("/sys/devices/system/cpu/cpu7/cpufreq/scaling_min_freq", "0");
  WriteFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", "2147483647");
  WriteFile("/sys/devices/system/cpu/cpu4/cpufreq/scaling_max_freq", "2147483647");
  WriteFile("/sys/devices/system/cpu/cpu7/cpufreq/scaling_max_freq", "2147483647");
  WriteFile("/sys/kernel/msm_performance/parameters/cpu_min_freq", "0:0 1:0 2:0 3:0 4:0 5:0 6:0 7:0");
}
inline void schedhorizon(){
        WriteFile("/sys/devices/system/cpu/cpufreq/policy0/scaling_governor", "schedhorizon");
		WriteFile("/sys/devices/system/cpu/cpufreq/policy4/scaling_governor", "schedhorizon");
		WriteFile("/sys/devices/system/cpu/cpufreq/policy7/scaling_governor", "schedhorizon");
}
inline void powersave_mode(){
    Log("更换模式为省电模式");
    schedhorizon();
	WriteFile(efficient_freq, "1700000");
	WriteFile(up_delay, "50");
	WriteFile(scaling_min_freq_limit, "1000000");
std::vector<std::string> freqs1To5 = {"1400000", "1700000", "2000000", "2500000"};
    std::vector<int> upDelays1To5 = {100, 100, 300, 500};
    int minFreqLimit1To5 = 800000;
    int policyStart1To5 = 1;
    int policyEnd1To5 = 5;
    std::vector<std::string> freqs6To7 = {"1200000", "1800000", "2500000"};
    std::vector<int> upDelays6To7 = {100, 500, 500};
    int minFreqLimit6To7 = 100000;
    int policyStart6To7 = 6;
    int policyEnd6To7 = 7;
    writePolicyValues(policyStart1To5, policyEnd1To5, freqs1To5, upDelays1To5, minFreqLimit1To5);
    writePolicyValues(policyStart6To7, policyEnd6To7, freqs6To7, upDelays6To7, minFreqLimit6To7);
	WriteFile(cpu_uclamp_min, "0");
	WriteFile(cpu_uclamp_max, "80");
	WriteFile(foreground_cpu_uclamp_min, "0");
	WriteFile(foreground_cpu_uclamp_max, "65");
    std::vector<std::string> paths = GpuDDR();
    for (const auto& path : paths) {
        WriteFile(path, "60000"); // 60℃ 
    }
}
inline void balance_mode(){
    Log("更换模式为均衡模式");
    schedhorizon();
	WriteFile(efficient_freq, "1700000");
	WriteFile(up_delay, "50");
	WriteFile(scaling_min_freq_limit, "1400000");
std::vector<std::string> freqs1To5 = {"1400000", "1700000", "2000000", "2500000"};
    std::vector<int> upDelays1To5 = {50, 50, 50, 100};
    int minFreqLimit1To5 = 1400000;
    int policyStart1To5 = 1;
    int policyEnd1To5 = 5;
    std::vector<std::string> freqs6To7 = {"1200000", "1800000", "2500000"};
    std::vector<int> upDelays6To7 = {50, 100, 100};
    int minFreqLimit6To7 = 1200000;
    int policyStart6To7 = 6;
    int policyEnd6To7 = 7;
    writePolicyValues(policyStart1To5, policyEnd1To5, freqs1To5, upDelays1To5, minFreqLimit1To5);
    writePolicyValues(policyStart6To7, policyEnd6To7, freqs6To7, upDelays6To7, minFreqLimit6To7);
	WriteFile(cpu_uclamp_min, "10");
	WriteFile(cpu_uclamp_max, "max");
	WriteFile(foreground_cpu_uclamp_min, "10");
	WriteFile(foreground_cpu_uclamp_max, "80");
      std::vector<std::string> paths = GpuDDR();
    for (const auto& path : paths) {
        WriteFile(path, "75000"); // 75℃ 
    }
}
/* 鸽
inline void MTK_GPU(){

}*/
inline void performance_mode(){
    Log("更换模式为性能模式");
    schedhorizon();
	WriteFile(efficient_freq, "0");
	WriteFile(up_delay, "0");
	WriteFile(scaling_min_freq_limit, "1700000");
std::vector<std::string> freqs1To5 = {"1400000", "1700000", "2000000", "2500000"};
    std::vector<int> upDelays1To5 = {50, 50, 50, 100};
    int minFreqLimit1To5 = 2000000;
    int policyStart1To5 = 1;
    int policyEnd1To5 = 5;
    std::vector<std::string> freqs6To7 = {"1200000", "1800000", "2500000"};
    std::vector<int> upDelays6To7 = {50, 100, 100};
    int minFreqLimit6To7 = 1800000;
    int policyStart6To7 = 6;
    int policyEnd6To7 = 7;
    writePolicyValues(policyStart1To5, policyEnd1To5, freqs1To5, upDelays1To5, minFreqLimit1To5);
    writePolicyValues(policyStart6To7, policyEnd6To7, freqs6To7, upDelays6To7, minFreqLimit6To7);
	WriteFile(cpu_uclamp_min, "10");
	WriteFile(cpu_uclamp_max, "max");
	WriteFile(foreground_cpu_uclamp_min, "10");
	WriteFile(foreground_cpu_uclamp_max, "80");
      std::vector<std::string> paths = GpuDDR();
    for (const auto& path : paths) {
        WriteFile(path, "130000"); // 130℃ 
    }
}
inline void fast_mode(){
    std::ifstream file(json_path);
    json data;
    // 将文件内容解析为json对象
    file >> data;
if (data.contains("Enable_Feas") && data["Enable_Feas"] == true) {
    enableFeas();
     } else {
    Log("更换模式为极速模式");
    schedhorizon();
	WriteFile(efficient_freq, "0");
	WriteFile(up_delay, "0");
	WriteFile(scaling_min_freq_limit, "1700000");
std::vector<std::string> freqs1To5 = {"1400000", "1700000", "2000000", "2500000"};
    std::vector<int> upDelays1To5 = {50, 50, 50, 100};
    int minFreqLimit1To5 = 2300000;
    int policyStart1To5 = 1;
    int policyEnd1To5 = 5;
    std::vector<std::string> freqs6To7 = {"1200000", "1800000", "2500000"};
    std::vector<int> upDelays6To7 = {50, 100, 100};
    int minFreqLimit6To7 = 2300000;
    int policyStart6To7 = 6;
    int policyEnd6To7 = 7;
    writePolicyValues(policyStart1To5, policyEnd1To5, freqs1To5, upDelays1To5, minFreqLimit1To5);
    writePolicyValues(policyStart6To7, policyEnd6To7, freqs6To7, upDelays6To7, minFreqLimit6To7);
	WriteFile(cpu_uclamp_min, "20");
	WriteFile(cpu_uclamp_max, "max");
	WriteFile(foreground_cpu_uclamp_min, "10");
	WriteFile(foreground_cpu_uclamp_max, "80");
    WriteFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", "2147483647");
    WriteFile("/sys/devices/system/cpu/cpu4/cpufreq/scaling_max_freq", "2147483647");
    WriteFile("/sys/devices/system/cpu/cpu7/cpufreq/scaling_max_freq", "2147483647");
      std::vector<std::string> paths = GpuDDR();
    for (const auto& path : paths) {
        WriteFile(path, "150000"); // 150℃ 
    }
    }
}
inline void Getconfig(const std::string& config_path) {
    std::ifstream file(config_path);
    std::string line;
    while (std::getline(file, line)) { // 按行读取文件
        if (line == "powersave") {
            powersave_mode();
        } else if (line == "balance") {
            balance_mode();
        } else if (line == "performance") {
		    performance_mode();
        } else if (line == "fast") {
            fast_mode();
       }
   }
}
inline void clear_log(){
	std::ofstream ofs;
    ofs.open(Log_path, std::ofstream::out | std::ofstream::trunc);
    ofs.close();
}
inline void Getjson(){
    std::ifstream file(json_path);
    json data;

    // 将文件内容解析为json对象
    file >> data;

    // 检查json中的特定键是否存在，并根据其值决定是否调用函数
    if (data.contains("Disable_qcom_GpuBoost") && data["Disable_qcom_GpuBoost"] == true) {
         disable_qcomGpuBoost();
    } 
    if (data.contains("Core_allocation") && data["Core_allocation"] == true) {
         core_allocation();
    } 
}