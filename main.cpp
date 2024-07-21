#include "utils.hpp"

int main(){
	//Init(); // 检测进程是否多开并进行初始化
	clear_log(); // 清空日志
	schedhorizon();
	Log("目前工作模式:schedhorizon 配置来源于@CoolAPK:ZTC1997");
	usleep(1000 * 100); // 100us
	Getjson();
    core_allocation();
	Getconfig(config_path); // 先进行获取配置文件内容再进行调整调度
/*这里我为什么要使用循环呢？因为配置文件是动态的 所以需要不断检测配置文件变化 然后进行调整调度 PS:推荐使用Scene 可以自动根据应用前台自动更改配置文件*/
while (true){
	sleep(1); // 上sleep 1s 防止cpu瞬时占用过高
	InotifyMain("/sdcard/Android/MW_CpuSpeedController/config.txt", IN_MODIFY); // 检测配置文件变化
	Getconfig(config_path); // 获取配置文件内容再进行调整调度
	}
}