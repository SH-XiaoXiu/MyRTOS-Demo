//
// Created by XiaoXiu on 8/30/2025.
//
// Platform: GD32F4xx
//

#include "MyRTOS.h"
#include "MyRTOS_Config.h"
#include "MyRTOS_Console.h"
#include "MyRTOS_Platform.h"
#include "MyRTOS_Driver_Timer.h"
#include "MyRTOS_Terminal.h"

#if (MY_RTOS_USE_LOG == 1)
#include "MyRTOS_Log.h"
#endif

/**
 * @brief 统一的系统初始化函数
 *        用户在 main 中只需要调用这一个函数
 */
void MyRTOS_SystemInit(void) {
    //最底层的板级支持初始化 (例如: 调试串口)
    MyRTOS_Platform_Init();
    //硬件驱动初始化 (例如: 定时器)
#if (defined(MY_RTOS_TIMER_DEVICE_LIST))
    MyRTOS_Timer_Init();
#endif
    //内核初始化
    MyRTOS_Init();
    //服务层初始化
#if (MY_RTOS_USE_CONSOLE == 1)
    MyRTOS_Console_Init();
#endif
#if (MY_RTOS_USE_LOG == 1)
    MyRTOS_Log_Init();
#endif
#if (MY_RTOS_USE_TERMINAL == 1)
    MyRTOS_Terminal_Init();
#endif
}
