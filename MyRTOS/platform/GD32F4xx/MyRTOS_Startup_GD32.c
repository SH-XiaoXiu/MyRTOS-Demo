#include "MyRTOS.h"
#include "MyRTOS_Config.h"
#include "MyRTOS_Platform.h"
#include "MyRTOS_Driver_Timer.h"

// 包含所有需要初始化的服务头文件
#if (MY_RTOS_USE_STDIO == 1)
#include "MyRTOS_IO.h"
#endif
#if (MY_RTOS_USE_LOG == 1)
#include "MyRTOS_Log.h"
#endif
#if (MY_RTOS_USE_SHELL == 1)
#include "MyRTOS_Shell.h"
#endif


/**
 * @brief 统一的系统初始化函数
 *        用户在 main 中只需要调用这一个函数。
 *        它会根据配置文件，按照正确的依赖顺序初始化所有模块。
 */
void MyRTOS_SystemInit(void) {
    MyRTOS_Platform_Init();
#if (defined(MY_RTOS_TIMER_DEVICE_LIST))
    MyRTOS_Timer_Init();
#endif
#if (MY_RTOS_USE_STDIO == 1)
    MyRTOS_StdIO_Init();
#endif
#if (MY_RTOS_USE_LOG == 1)
    MyRTOS_Log_Init();
#endif
#if (MY_RTOS_USE_SHELL == 1)
    MyRTOS_Shell_Init();
#endif
}